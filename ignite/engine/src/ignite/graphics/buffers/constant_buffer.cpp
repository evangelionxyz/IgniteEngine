// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "constant_buffer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/renderer.hpp"

namespace ignite
{
    ConstantBuffer::ConstantBuffer(const size_t size, bool isVolatile, const uint32_t maxVersion, const std::string &debugName)
    {
        IGN_PROFILE_FUNCTION();
        (void)isVolatile;
        (void)maxVersion;

        m_ByteSize = size;
        auto *deviceManager = DeviceManager::GetInstance();
        nri::Device *device = deviceManager->GetDevice();
        const auto &nri = deviceManager->GetCoreInterface();

        // Uniform buffer allocations should align to 256 bytes for hardware compatibility
        uint64_t alignedSize = (size + 255) & ~255ULL;

        nri::BufferDesc cbDesc = {};
        cbDesc.size = alignedSize;
        cbDesc.usage = nri::BufferUsageBits::CONSTANT;

        nri::Result result = nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_UPLOAD, 1.0f, cbDesc, m_Handle);
        LOG_ASSERT(result == nri::Result::SUCCESS && m_Handle, "Failed to create constant buffer!");
        if (m_Handle)
        {
            nri.SetDebugName((nri::Object*)m_Handle, debugName.c_str());
            IGN_PROFILE_ALLOC_N(m_Handle, size, "GPU Constant Buffer");
            Renderer::Stats.gpuConstantBufferBytes += size;

            nri::BufferViewDesc viewDesc = {};
            viewDesc.buffer = m_Handle;
            viewDesc.type = nri::BufferView::CONSTANT_BUFFER;
            viewDesc.offset = 0;
            viewDesc.size = alignedSize;
            nri::Result viewRes = nri.CreateBufferView(viewDesc, m_Descriptor);
            LOG_ASSERT(viewRes == nri::Result::SUCCESS && m_Descriptor, "Failed to create constant buffer view!");
        }
    }

    ConstantBuffer::~ConstantBuffer()
    {
        auto *deviceManager = DeviceManager::GetInstance();
        if (m_Descriptor)
        {
            if (deviceManager)
            {
                deviceManager->GetCoreInterface().DestroyDescriptor(m_Descriptor);
            }
            m_Descriptor = nullptr;
        }
        if (m_Handle)
        {
            IGN_PROFILE_FREE_N(m_Handle, "GPU Constant Buffer");
            Renderer::Stats.gpuConstantBufferBytes -= m_ByteSize;
            if (deviceManager)
            {
                deviceManager->GetCoreInterface().DestroyBuffer(m_Handle);
            }
            m_Handle = nullptr;
        }
    }

    void ConstantBuffer::SetData(const void *data, size_t dataSize, const size_t offset)
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

    Ref<ConstantBuffer> ConstantBuffer::Create(const size_t size, bool isVolatile, const uint32_t maxVersion, const std::string &debugName)
    {
        return CreateRef<ConstantBuffer>(size, isVolatile, maxVersion, debugName);
    }
}
