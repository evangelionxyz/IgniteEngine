// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "frame_context.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    namespace
    {
        void CreateHostUploadStructuredBuffer(
            nri::Device *device,
            size_t byteSize,
            uint32_t structStride,
            const char *debugName,
            nri::Buffer *&outBuffer,
            nri::Memory *&outMemory,
            nri::Descriptor *&outDescriptor)
        {
            auto *dm = DeviceManager::GetInstance();
            if (!device && dm)
                device = dm->GetDevice();
            LOG_ASSERT(device && dm, "[FrameContext] Device is null");

            const auto &nri = dm->GetCoreInterface();

            nri::BufferDesc bufferDesc = {};
            bufferDesc.size = byteSize;
            bufferDesc.structureStride = structStride;
            bufferDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;

            nri::Result res = nri.CreateBuffer(*device, bufferDesc, outBuffer);
            LOG_ASSERT(res == nri::Result::SUCCESS && outBuffer, "[FrameContext] Failed to create {}", debugName);
            nri.SetDebugName(outBuffer, debugName);

            nri::MemoryDesc memDesc = {};
            nri.GetBufferMemoryDesc(*outBuffer, nri::MemoryLocation::HOST_UPLOAD, memDesc);

            nri::AllocateMemoryDesc allocDesc = {};
            allocDesc.size = memDesc.size;
            allocDesc.type = memDesc.type;
            res = nri.AllocateMemory(*device, allocDesc, outMemory);
            LOG_ASSERT(res == nri::Result::SUCCESS && outMemory, "[FrameContext] Failed to allocate memory for {}", debugName);

            nri::BindBufferMemoryDesc bindDesc = {};
            bindDesc.buffer = outBuffer;
            bindDesc.memory = outMemory;
            bindDesc.offset = 0;
            res = nri.BindBufferMemory(&bindDesc, 1);
            LOG_ASSERT(res == nri::Result::SUCCESS, "[FrameContext] Failed to bind memory for {}", debugName);

            nri::BufferViewDesc viewDesc = {};
            viewDesc.buffer = outBuffer;
            viewDesc.type = nri::BufferView::STRUCTURED_BUFFER;
            viewDesc.offset = 0;
            viewDesc.size = byteSize;
            viewDesc.structureStride = structStride;
            res = nri.CreateBufferView(viewDesc, outDescriptor);
            LOG_ASSERT(res == nri::Result::SUCCESS && outDescriptor, "[FrameContext] Failed to create descriptor for {}", debugName);
        }

        void DestroyStructuredBuffer(nri::Buffer *&buffer, nri::Memory *&memory, nri::Descriptor *&descriptor)
        {
            auto *dm = DeviceManager::GetInstance();
            if (!dm || !dm->GetDevice())
                return;

            const auto &nri = dm->GetCoreInterface();
            if (descriptor)
            {
                nri.DestroyDescriptor(descriptor);
                descriptor = nullptr;
            }
            if (buffer)
            {
                nri.DestroyBuffer(buffer);
                buffer = nullptr;
            }
            if (memory)
            {
                nri.FreeMemory(memory);
                memory = nullptr;
            }
        }
    }

    uint32_t ObjectAllocator::Allocate(nri::CommandBuffer *cmd, const Mesh_GPUData &data)
    {
        (void)cmd;
        const uint32_t index = m_ObjectCount++;
        const uint32_t maxObjects = m_Buffer ? static_cast<uint32_t>(m_ByteSize / sizeof(Mesh_GPUData)) : 0;
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

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        void *mapped = nri.MapBuffer(*m_Buffer, static_cast<uint64_t>(index) * sizeof(Mesh_GPUData), sizeof(Mesh_GPUData));
        if (mapped)
        {
            std::memcpy(mapped, &data, sizeof(Mesh_GPUData));
            nri.UnmapBuffer(*m_Buffer);
        }
        return index;
    }

    uint32_t InstanceIndexAllocator::Allocate(nri::CommandBuffer *cmd, const uint32_t *indices, uint32_t count)
    {
        (void)cmd;
        if (count == 0)
            return 0;

        const uint32_t base = m_IndexCount;
        const uint32_t maxIndices = m_Buffer ? static_cast<uint32_t>(m_ByteSize / sizeof(uint32_t)) : 0;
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

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const uint64_t copySize = static_cast<uint64_t>(count) * sizeof(uint32_t);
        void *mapped = nri.MapBuffer(*m_Buffer, static_cast<uint64_t>(base) * sizeof(uint32_t), copySize);
        if (mapped)
        {
            std::memcpy(mapped, indices, copySize);
            nri.UnmapBuffer(*m_Buffer);
        }
        m_IndexCount += count;
        return base;
    }

    void InstanceIndexBuffer::Initialize(uint32_t maxIndices, nri::Device *device)
    {
        m_ByteSize = sizeof(uint32_t) * maxIndices;
        CreateHostUploadStructuredBuffer(device, m_ByteSize, sizeof(uint32_t), "Instance Index Buffer", m_Buffer, m_Memory, m_Descriptor);
    }

    void InstanceIndexBuffer::Shutdown()
    {
        DestroyStructuredBuffer(m_Buffer, m_Memory, m_Descriptor);
        m_ByteSize = 0;
    }

    uint32_t BoneAllocator::Allocate(nri::CommandBuffer *cmd, const glm::mat4 *bones, uint32_t count)
    {
        (void)cmd;
        const uint32_t index = m_BoneCount;
        m_BoneCount += count;
        const uint32_t maxBones = m_Buffer ? static_cast<uint32_t>(m_ByteSize / sizeof(glm::mat4)) : 0;
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

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const uint64_t copySize = static_cast<uint64_t>(count) * sizeof(glm::mat4);
        void *mapped = nri.MapBuffer(*m_Buffer, static_cast<uint64_t>(index) * sizeof(glm::mat4), copySize);
        if (mapped)
        {
            std::memcpy(mapped, bones, copySize);
            nri.UnmapBuffer(*m_Buffer);
        }
        return index;
    }

    void ObjectBuffer::Initialize(uint32_t maxObjects, nri::Device *device)
    {
        m_ByteSize = sizeof(Mesh_GPUData) * maxObjects;
        CreateHostUploadStructuredBuffer(device, m_ByteSize, sizeof(Mesh_GPUData), "Object Buffer", m_Buffer, m_Memory, m_Descriptor);
    }

    void ObjectBuffer::Shutdown()
    {
        DestroyStructuredBuffer(m_Buffer, m_Memory, m_Descriptor);
        m_ByteSize = 0;
    }

    void BoneBuffer::Initialize(uint32_t maxBones, nri::Device *device)
    {
        m_ByteSize = sizeof(glm::mat4) * maxBones;
        CreateHostUploadStructuredBuffer(device, m_ByteSize, sizeof(glm::mat4), "Bone Buffer", m_Buffer, m_Memory, m_Descriptor);
    }

    void BoneBuffer::Shutdown()
    {
        DestroyStructuredBuffer(m_Buffer, m_Memory, m_Descriptor);
        m_ByteSize = 0;
    }

    FrameContext::FrameContext(nri::Device *device)
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
        // 16384 objects, 4096 bone slots * MAX_BONES, 65536 instance indices (for batched draws)
        objectBuffer.Initialize(16384, device);
        boneBuffer.Initialize(4096 * MAX_BONES, device);
        instanceIndexBuffer.Initialize(65536, device);
    }

    void FrameContext::InitializeBindingSets(nri::Device *device, const BindingLayoutDesc &staticLayout, const BindingLayoutDesc &animLayout)
    {
        staticPipelineLayout = staticLayout.CreatePipelineLayout(nri::StageBits::ALL, device);
        animPipelineLayout = animLayout.CreatePipelineLayout(nri::StageBits::ALL, device);

        // Static
        {
            BindingSetDesc desc;
            desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
            desc.AddItem(BindingSetItem::ConstantBuffer(1, cameraBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(2, objectBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(3, instanceIndexBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(4, sceneBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(5, csmBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetDescriptor()));
            staticMeshBindingSet = BindingSet::Create("FrameContext StaticMesh", BindingSet::AllocateAndUpdate(staticPipelineLayout, 0, desc));
        }

        // Animated
        {
            BindingSetDesc desc;
            desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
            desc.AddItem(BindingSetItem::ConstantBuffer(1, cameraBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(2, objectBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(3, boneBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(4, sceneBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(5, csmBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetDescriptor()));
            animatedBindingSet = BindingSet::Create("FrameContext AnimatedMesh", BindingSet::AllocateAndUpdate(animPipelineLayout, 0, desc));
        }

        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            // Static CSM
            {
                BindingSetDesc desc;
                desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
                desc.AddItem(BindingSetItem::ConstantBuffer(1, cameraBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(2, objectBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(3, instanceIndexBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(4, sceneBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(5, csmPerCascadeBuffers[i].GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetDescriptor()));
                staticMeshCSMBindingSet[i] = BindingSet::Create("FrameContext StaticCSM", BindingSet::AllocateAndUpdate(staticPipelineLayout, 0, desc));
            }

            // Animated CSM
            {
                BindingSetDesc desc;
                desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
                desc.AddItem(BindingSetItem::ConstantBuffer(1, cameraBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(2, objectBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(3, boneBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(4, sceneBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(5, csmPerCascadeBuffers[i].GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(6, pointLightBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(7, spotLightBuffer.GetDescriptor()));
                animatedMeshCSMBindingSet[i] = BindingSet::Create("FrameContext AnimatedCSM", BindingSet::AllocateAndUpdate(animPipelineLayout, 0, desc));
            }
        }
    }

    void FrameContext::Shutdown()
    {
        staticMeshBindingSet.reset();
        animatedBindingSet.reset();
        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            staticMeshCSMBindingSet[i].reset();
            animatedMeshCSMBindingSet[i].reset();
        }

        objectBuffer.Shutdown();
        boneBuffer.Shutdown();
        instanceIndexBuffer.Shutdown();

        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            const auto &nri = dm->GetCoreInterface();
            if (staticPipelineLayout)
            {
                nri.DestroyPipelineLayout(staticPipelineLayout);
                staticPipelineLayout = nullptr;
            }
            if (animPipelineLayout)
            {
                nri.DestroyPipelineLayout(animPipelineLayout);
                animPipelineLayout = nullptr;
            }
        }
    }
}
