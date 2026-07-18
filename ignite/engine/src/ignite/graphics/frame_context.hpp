// Copyritght (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_FRAME_CONTEXT_HPP
#define IGN_FRAME_CONTEXT_HPP

#include "buffers/constant_buffer.hpp"
#include "gpu_data.hpp"

#include <nvrhi/nvrhi.h>

namespace ignite
{

	class ObjectAllocator
	{
	public:
		void BeginFrame() { m_ObjectCount = 0; }
		uint32_t Allocate(nvrhi::ICommandList *cmd, const Mesh_GPUData &data);
		void SetBuffer(nvrhi::BufferHandle buffer) { m_Buffer = buffer; }

	private:
		nvrhi::BufferHandle m_Buffer;
		uint32_t m_ObjectCount = 0;
	};

	class BoneAllocator
	{
	public:
		void BeginFrame() { m_BoneCount = 0; }
		uint32_t Allocate(nvrhi::ICommandList *cmd, const glm::mat4 *bones, uint32_t count);
		void SetBuffer(nvrhi::BufferHandle buffer) { m_Buffer = buffer; }

	private:
		nvrhi::BufferHandle m_Buffer;
		uint32_t m_BoneCount = 0;
	};

	class ObjectBuffer
	{
	public:
		void Initialize(uint32_t maxObjects, nvrhi::IDevice *device);
		nvrhi::BufferHandle GetHandle() const { return m_Buffer; }

	private:
		nvrhi::BufferHandle m_Buffer;
	};

	class BoneBuffer
	{
	public:
		void Initialize(uint32_t maxBones, nvrhi::IDevice *device);
		nvrhi::BufferHandle GetHandle() const { return m_Buffer; }

	private:
		nvrhi::BufferHandle m_Buffer;
	};

    class FrameContext
    {
	public:
		FrameContext(nvrhi::IDevice *device);
		void InitializeBindingSets(nvrhi::IDevice *device, nvrhi::IBindingLayout *staticLayout, nvrhi::IBindingLayout *animLayout);

	public:
		ConstantBuffer cameraBuffer;
		ConstantBuffer sceneBuffer;
		ConstantBuffer csmBuffer;
		ConstantBuffer pointLightBuffer;
		ConstantBuffer spotLightBuffer;

		ObjectBuffer objectBuffer;
		ObjectAllocator objectAllocator;

		BoneBuffer boneBuffer;
		BoneAllocator boneAllocator;

		nvrhi::BindingSetHandle staticMeshBindingSet;
		nvrhi::BindingSetHandle animatedBindingSet;	

		ConstantBuffer csmPerCascadeBuffers[NUM_CASCADES];
		nvrhi::BindingSetHandle staticMeshCSMBindingSet[NUM_CASCADES];
		nvrhi::BindingSetHandle animatedMeshCSMBindingSet[NUM_CASCADES];
	};
}

#endif
