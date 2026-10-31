// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "vertex_buffer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    VertexBuffer::VertexBuffer(const size_t size, const std::string &debugName)
    {
        IGN_PROFILE_FUNCTION();
        m_ByteSize = size;

        auto *deviceManager = DeviceManager::GetInstance();
        nri::Device *device = deviceManager->GetDevice();
        const auto &nri = deviceManager->GetCoreInterface();

        nri::BufferDesc desc = {};
        desc.size = size;
        desc.usage = nri::BufferUsageBits::VERTEX;

        nri::Result result = nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_UPLOAD, 1.0f, desc, m_Handle);
        LOG_ASSERT(result == nri::Result::SUCCESS && m_Handle, "[Vertex Buffer] Failed to create handle!");
        if (m_Handle)
        {
            nri.SetDebugName((nri::Object*)m_Handle, debugName.c_str());
            IGN_PROFILE_ALLOC_N(m_Handle, size, "GPU Vertex Buffer");
            Renderer::Stats.gpuVertexBufferBytes += size;
        }
    }

    VertexBuffer::~VertexBuffer()
    {
        if (m_Handle)
        {
            IGN_PROFILE_FREE_N(m_Handle, "GPU Vertex Buffer");
            Renderer::Stats.gpuVertexBufferBytes -= m_ByteSize;
            auto *deviceManager = DeviceManager::GetInstance();
            if (deviceManager)
            {
                deviceManager->GetCoreInterface().DestroyBuffer(m_Handle);
            }
            m_Handle = nullptr;
        }
    }

    void VertexBuffer::SetData(const void *data, size_t dataSize, size_t offset) const
    {
        if (!m_Handle || !data || dataSize == 0) return;

        auto *deviceManager = DeviceManager::GetInstance();
        const auto &nri = deviceManager->GetCoreInterface();
        void *mapped = nri.MapBuffer(*m_Handle, offset, dataSize);
        if (mapped)
        {
            memcpy(mapped, data, dataSize);
            nri.UnmapBuffer(*m_Handle);
        }
    }

    Ref<VertexBuffer> VertexBuffer::Create(size_t size, const std::string &debugName)
    {
        return CreateRef<VertexBuffer>(size, debugName);
    }
}
