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
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::BufferDesc desc;
        desc.byteSize = size;
        desc.isVertexBuffer = true;
        desc.keepInitialState = true;
        desc.initialState = nvrhi::ResourceStates::VertexBuffer;
        desc.debugName = debugName;

        m_ByteSize = size;
        m_Handle = device->createBuffer(desc);
        LOG_ASSERT(m_Handle, "[Vertex Buffer] Failed to create handle!");
        if (m_Handle)
        {
            IGN_PROFILE_ALLOC_N(m_Handle.Get(), size, "GPU Vertex Buffer");
            Renderer::Stats.gpuVertexBufferBytes += size;
        }
    }

    VertexBuffer::~VertexBuffer()
    {
        if (m_Handle)
        {
            IGN_PROFILE_FREE_N(m_Handle.Get(), "GPU Vertex Buffer");
            Renderer::Stats.gpuVertexBufferBytes -= m_ByteSize;
            m_Handle = nullptr;
        }
    }

	void VertexBuffer::SetData(nvrhi::ICommandList *cmd, void *data, size_t dataSize, const size_t offset) const
    {
        cmd->writeBuffer(m_Handle, data, dataSize, offset);
    }

    Ref<VertexBuffer> VertexBuffer::Create(size_t size, const std::string &debugName)
    {
        return CreateRef<VertexBuffer>(size, debugName);
    }
}
