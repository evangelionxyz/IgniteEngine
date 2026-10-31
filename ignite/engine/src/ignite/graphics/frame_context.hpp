// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_FRAME_CONTEXT_HPP
#define IGN_FRAME_CONTEXT_HPP

#include "buffers/constant_buffer.hpp"
#include "binding_set.hpp"
#include "gpu_data.hpp"

#include <NRI.h>

namespace ignite
{
    class ObjectBuffer
    {
    public:
        void Initialize(uint32_t maxObjects, nri::Device *device);
        void Shutdown();
        nri::Buffer *GetHandle() const { return m_Buffer; }
        nri::Descriptor *GetDescriptor() const { return m_Descriptor; }
        size_t GetByteSize() const { return m_ByteSize; }

        operator nri::Buffer *() const { return m_Buffer; }

    private:
        nri::Buffer *m_Buffer = nullptr;
        nri::Memory *m_Memory = nullptr;
        nri::Descriptor *m_Descriptor = nullptr;
        size_t m_ByteSize = 0;
    };

    class BoneBuffer
    {
    public:
        void Initialize(uint32_t maxBones, nri::Device *device);
        void Shutdown();
        nri::Buffer *GetHandle() const { return m_Buffer; }
        nri::Descriptor *GetDescriptor() const { return m_Descriptor; }
        size_t GetByteSize() const { return m_ByteSize; }

        operator nri::Buffer *() const { return m_Buffer; }

    private:
        nri::Buffer *m_Buffer = nullptr;
        nri::Memory *m_Memory = nullptr;
        nri::Descriptor *m_Descriptor = nullptr;
        size_t m_ByteSize = 0;
    };

    class InstanceIndexBuffer
    {
    public:
        void Initialize(uint32_t maxIndices, nri::Device *device);
        void Shutdown();
        nri::Buffer *GetHandle() const { return m_Buffer; }
        nri::Descriptor *GetDescriptor() const { return m_Descriptor; }
        size_t GetByteSize() const { return m_ByteSize; }

        operator nri::Buffer *() const { return m_Buffer; }

    private:
        nri::Buffer *m_Buffer = nullptr;
        nri::Memory *m_Memory = nullptr;
        nri::Descriptor *m_Descriptor = nullptr;
        size_t m_ByteSize = 0;
    };

    class ObjectAllocator
    {
    public:
        void BeginFrame() { m_ObjectCount = 0; }
        uint32_t Allocate(nri::CommandBuffer *cmd, const Mesh_GPUData &data);
        void SetBuffer(const ObjectBuffer &buffer) { m_Buffer = buffer.GetHandle(); m_ByteSize = buffer.GetByteSize(); }

        operator nri::Buffer *() const { return m_Buffer; }

    private:
        nri::Buffer *m_Buffer = nullptr;
        size_t m_ByteSize = 0;
        uint32_t m_ObjectCount = 0;
    };

    class BoneAllocator
    {
    public:
        void BeginFrame() { m_BoneCount = 0; }
        uint32_t Allocate(nri::CommandBuffer *cmd, const glm::mat4 *bones, uint32_t count);
        void SetBuffer(const BoneBuffer &buffer) { m_Buffer = buffer.GetHandle(); m_ByteSize = buffer.GetByteSize(); }

        operator nri::Buffer *() const { return m_Buffer; }

    private:
        nri::Buffer *m_Buffer = nullptr;
        size_t m_ByteSize = 0;
        uint32_t m_BoneCount = 0;
    };

    class InstanceIndexAllocator
    {
    public:
        void BeginFrame() { m_IndexCount = 0; }
        uint32_t Allocate(nri::CommandBuffer *cmd, const uint32_t *indices, uint32_t count);
        void SetBuffer(const InstanceIndexBuffer &buffer) { m_Buffer = buffer.GetHandle(); m_ByteSize = buffer.GetByteSize(); }

        operator nri::Buffer *() const { return m_Buffer; }

    private:
        nri::Buffer *m_Buffer = nullptr;
        size_t m_ByteSize = 0;
        uint32_t m_IndexCount = 0;
    };

    class FrameContext
    {
    public:
        FrameContext(nri::Device *device);
        void InitializeBindingSets(nri::Device *device, const BindingLayoutDesc &staticLayout, const BindingLayoutDesc &animLayout);
        void Shutdown();

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

        InstanceIndexBuffer instanceIndexBuffer;
        InstanceIndexAllocator instanceIndexAllocator;

        Ref<BindingSet> staticMeshBindingSet;
        Ref<BindingSet> animatedBindingSet;

        ConstantBuffer csmPerCascadeBuffers[NUM_CASCADES];
        Ref<BindingSet> staticMeshCSMBindingSet[NUM_CASCADES];
        Ref<BindingSet> animatedMeshCSMBindingSet[NUM_CASCADES];

        nri::PipelineLayout *staticPipelineLayout = nullptr;
        nri::PipelineLayout *animPipelineLayout = nullptr;

        uint32_t frameIndexInFlight = 0;
    };
}

#endif
