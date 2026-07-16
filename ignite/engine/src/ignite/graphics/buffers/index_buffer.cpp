// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "index_buffer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    IndexBuffer::IndexBuffer(size_t size, const std::string &debugName)
    {
        IGN_PROFILE_FUNCTION();
        m_Count = static_cast<uint32_t>(size) / sizeof(uint32_t);
        m_ByteSize = size;

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::BufferDesc desc;
        desc.byteSize = size;
        desc.isIndexBuffer = true;
        desc.keepInitialState = true;
        desc.initialState = nvrhi::ResourceStates::IndexBuffer;
        desc.debugName = debugName;

        m_Handle = device->createBuffer(desc);
        LOG_ASSERT(m_Handle, "[Index Buffer] Failed to create handle!");
        if (m_Handle)
        {
            IGN_PROFILE_ALLOC_N(m_Handle.Get(), size, "GPU Index Buffer");
            Renderer::Stats.gpuIndexBufferBytes += size;
        }
    }

    IndexBuffer::~IndexBuffer()
    {
        if (m_Handle)
        {
            IGN_PROFILE_FREE_N(m_Handle.Get(), "GPU Index Buffer");
            Renderer::Stats.gpuIndexBufferBytes -= m_ByteSize;
            m_Handle = nullptr;
        }
    }

    void IndexBuffer::SetData(nvrhi::ICommandList *cmd, Buffer buffer, size_t offset) const
    {
        IGN_PROFILE_SCOPE("IndexBuffer::SetData");
        cmd->writeBuffer(m_Handle, buffer.Data(), buffer.Size(), offset);
    }

    Ref<IndexBuffer> IndexBuffer::Create(size_t size, const std::string &debugName)
    {
        return CreateRef<IndexBuffer>(size, debugName);
    }
}
