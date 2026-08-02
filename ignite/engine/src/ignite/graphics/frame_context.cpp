// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "frame_context.hpp"
#include "ignite/scene/icamera.hpp"

#include "ignite/core/logger.hpp"

namespace ignite
{

	uint32_t ObjectAllocator::Allocate(nvrhi::ICommandList *cmd, const Mesh_GPUData &data)
	{
		const uint32_t index = m_ObjectCount++;
		const uint32_t maxObjects = m_Buffer ? static_cast<uint32_t>(m_Buffer->getDesc().byteSize / sizeof(Mesh_GPUData)) : 0;
		if (index >= maxObjects)
		{
			static bool loggedOnce = false;
			if (!loggedOnce)
			{
				LOG_ERROR("[ObjectAllocator] Overflow! Allocated: {}, Max: {}. Skipping GPU write to prevent corruption.", index + 1, maxObjects);
				loggedOnce = true;
			}
			return maxObjects > 0 ? (maxObjects - 1) : 0;
		}
		cmd->writeBuffer(m_Buffer, &data, sizeof(data),
			index * sizeof(Mesh_GPUData));
		return index;
	}

	uint32_t InstanceIndexAllocator::Allocate(nvrhi::ICommandList *cmd, const uint32_t *indices, uint32_t count)
	{
		if (count == 0)
			return 0;

		const uint32_t base = m_IndexCount;
		const uint32_t maxIndices = m_Buffer ? static_cast<uint32_t>(m_Buffer->getDesc().byteSize / sizeof(uint32_t)) : 0;
		if (base + count > maxIndices)
		{
			static bool loggedOnce = false;
			if (!loggedOnce)
			{
				LOG_ERROR("[InstanceIndexAllocator] Overflow! Requested: {}, Max: {}. Clamping.", base + count, maxIndices);
				loggedOnce = true;
			}
			return base < maxIndices ? base : 0;
		}
		cmd->writeBuffer(m_Buffer, indices, count * sizeof(uint32_t), base * sizeof(uint32_t));
		m_IndexCount += count;
		return base;
	}

	void InstanceIndexBuffer::Initialize(uint32_t maxIndices, nvrhi::IDevice *device)
	{
		nvrhi::BufferDesc desc;
		desc.byteSize = sizeof(uint32_t) * maxIndices;
		desc.canHaveUAVs = false;
		desc.canHaveTypedViews = false;
		desc.structStride = sizeof(uint32_t);
		desc.isVertexBuffer = false;
		desc.isIndexBuffer = false;
		desc.debugName = "Instance Index Buffer";
		desc.initialState = nvrhi::ResourceStates::ShaderResource;
		desc.keepInitialState = true;
		m_Buffer = device->createBuffer(desc);
	}

	uint32_t BoneAllocator::Allocate(nvrhi::ICommandList *cmd, const glm::mat4 *bones, uint32_t count)
	{
		const uint32_t index = m_BoneCount;
		m_BoneCount += count;
		const uint32_t maxBones = m_Buffer ? static_cast<uint32_t>(m_Buffer->getDesc().byteSize / sizeof(glm::mat4)) : 0;
		if (index + count > maxBones)
		{
			static bool loggedOnce = false;
			if (!loggedOnce)
			{
				LOG_ERROR("[BoneAllocator] Overflow! Allocated: {}, Max: {}. Skipping GPU write to prevent corruption.", index + count, maxBones);
				loggedOnce = true;
			}
			return 0;
		}
		cmd->writeBuffer(m_Buffer, bones,
			count * sizeof(glm::mat4),  // data size
			index * sizeof(glm::mat4)); // data offset
		return index;
	}

	void ObjectBuffer::Initialize(uint32_t maxObjects, nvrhi::IDevice *device)
	{
		nvrhi::BufferDesc desc;
		desc.byteSize = sizeof(Mesh_GPUData) * maxObjects;
		desc.canHaveUAVs = false;
		desc.canHaveTypedViews = false;
		desc.structStride = sizeof(Mesh_GPUData);
		desc.isVertexBuffer = false;
		desc.isIndexBuffer = false;
		desc.debugName = "Object Buffer";
		desc.initialState = nvrhi::ResourceStates::ShaderResource;
		desc.keepInitialState = true;
		m_Buffer = device->createBuffer(desc);
	}

	void BoneBuffer::Initialize(uint32_t maxBones, nvrhi::IDevice *device)
	{
		nvrhi::BufferDesc desc;
		desc.byteSize = sizeof(glm::mat4) * maxBones;
		desc.canHaveUAVs = false;
		desc.canHaveTypedViews = false;
		desc.structStride = sizeof(glm::mat4);
		desc.isVertexBuffer = false;
		desc.isIndexBuffer = false;
		desc.debugName = "Bone Buffer";
		desc.initialState = nvrhi::ResourceStates::ShaderResource;
		desc.keepInitialState = true;
		m_Buffer = device->createBuffer(desc);
	}

	FrameContext::FrameContext(nvrhi::IDevice *device)
		: cameraBuffer(sizeof(CameraBufferData), true, 16, "Camera Buffer")
		, sceneBuffer(sizeof(Scene_GPUData), true, 16, "Scene Buffer")
		, csmBuffer(sizeof(CSM_GPUData), true, 16, "CSM Buffer")
		, pointLightBuffer(sizeof(PointLightBufferData), true, 16, "Point Light Buffer")
		, spotLightBuffer(sizeof(SpotLightBufferData), true, 16, "Spot Light Buffer")
		, csmPerCascadeBuffers{
			ConstantBuffer(sizeof(CSM_GPUData), true, 16, "CSM Cascade 0 Buffer"),
			ConstantBuffer(sizeof(CSM_GPUData), true, 16, "CSM Cascade 1 Buffer"),
			ConstantBuffer(sizeof(CSM_GPUData), true, 16, "CSM Cascade 2 Buffer"),
			ConstantBuffer(sizeof(CSM_GPUData), true, 16, "CSM Cascade 3 Buffer")
		}
	{
		// 16384 objects, 16384 bone slots * MAX_BONES, 65536 instance indices (for batched draws)
		objectBuffer.Initialize(16384, device);
		boneBuffer.Initialize(4096 * MAX_BONES, device);
		instanceIndexBuffer.Initialize(65536, device);
	}

	void FrameContext::InitializeBindingSets(nvrhi::IDevice *device, nvrhi::IBindingLayout *staticLayout, nvrhi::IBindingLayout *animLayout)
	{
		// Static
		{
			nvrhi::BindingSetDesc desc;
			desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(uint32_t)));
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cameraBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, objectBuffer.GetHandle()));
			desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(3, instanceIndexBuffer.GetHandle())); // Instance index indirection
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, sceneBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, csmBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetHandle())); // volatile
			staticMeshBindingSet = device->createBindingSet(desc, staticLayout);
		}

		// Animated
		{
			nvrhi::BindingSetDesc desc;
			desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(uint32_t)));
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cameraBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, objectBuffer.GetHandle()));
			desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(3, boneBuffer.GetHandle()));
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, sceneBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, csmBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetHandle())); // volatile
			animatedBindingSet = device->createBindingSet(desc, animLayout);
		}

		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			// Static CSM
			{
				nvrhi::BindingSetDesc desc;
				desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(uint32_t)));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cameraBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, objectBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(3, instanceIndexBuffer.GetHandle())); // Instance index indirection
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, sceneBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, csmPerCascadeBuffers[i].GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetHandle()));
				staticMeshCSMBindingSet[i] = device->createBindingSet(desc, staticLayout);
			}

			// Animated CSM (bones stay at t3; no instance index buffer needed for skeletal — deferred)
			{
				nvrhi::BindingSetDesc desc;
				desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(uint32_t)));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cameraBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, objectBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(3, boneBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, sceneBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, csmPerCascadeBuffers[i].GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetHandle()));
				animatedMeshCSMBindingSet[i] = device->createBindingSet(desc, animLayout);
			}
		}
	}
}
