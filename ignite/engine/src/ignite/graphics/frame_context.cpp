// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "frame_context.hpp"
#include "ignite/scene/icamera.hpp"

namespace ignite
{

	uint32_t ObjectAllocator::Allocate(nvrhi::ICommandList *cmd, const Mesh_GPUData &data)
	{
		const uint32_t index = m_ObjectCount++;
		cmd->writeBuffer(m_Buffer, &data, sizeof(data),
			index * sizeof(Mesh_GPUData));
		return index;
	}

	uint32_t BoneAllocator::Allocate(nvrhi::ICommandList *cmd, const glm::mat4 *bones, uint32_t count)
	{
		const uint32_t index = m_BoneCount;
		m_BoneCount += count;
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
		objectBuffer.Initialize(1024, device);
		boneBuffer.Initialize(1024 * MAX_BONES, device);
	}

	void FrameContext::InitializeBindingSets(nvrhi::IDevice *device, nvrhi::IBindingLayout *staticLayout, nvrhi::IBindingLayout *animLayout)
	{
		// Static
		{
			nvrhi::BindingSetDesc desc;
			desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(uint32_t)));
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cameraBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(2, objectBuffer.GetHandle()));
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, sceneBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, csmBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, pointLightBuffer.GetHandle())); // volatile
			desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, spotLightBuffer.GetHandle())); // volatile
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
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, sceneBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, csmPerCascadeBuffers[i].GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, pointLightBuffer.GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, spotLightBuffer.GetHandle()));
				staticMeshCSMBindingSet[i] = device->createBindingSet(desc, staticLayout);
			}

			// Animated CSM
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
