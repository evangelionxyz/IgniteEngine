#include "constant_buffer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    ConstantBuffer::ConstantBuffer(const size_t size, bool isVolatile, const uint32_t maxVersion, const std::string &debugName)
    {
        IGN_PROFILE_FUNCTION();
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::BufferDesc cbDesc;
        cbDesc.byteSize = size;
        cbDesc.isConstantBuffer = true;
        cbDesc.isVolatile = isVolatile;
        cbDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        cbDesc.keepInitialState = true;
        cbDesc.maxVersions = maxVersion;
        cbDesc.debugName = debugName;

        m_Handle = device->createBuffer(cbDesc);
        LOG_ASSERT(m_Handle, "Failed to create constant buffer!");
        if (m_Handle)
        {
            IGN_PROFILE_ALLOC_N(m_Handle.Get(), size, "GPU Constant Buffer");
        }
    }

    ConstantBuffer::~ConstantBuffer()
    {
        if (m_Handle)
        {
            IGN_PROFILE_FREE_N(m_Handle.Get(), "GPU Constant Buffer");
            m_Handle = nullptr;
        }
    }
    
	void ConstantBuffer::SetData(nvrhi::ICommandList *cmd, Buffer buffer, const size_t offset)
    {
        IGN_PROFILE_SCOPE("ConstantBuffer::SetData");
        cmd->writeBuffer(m_Handle, buffer.data, buffer.size, offset);
    }

    Ref<ConstantBuffer> ConstantBuffer::Create(const size_t size, bool isVolatile, const uint32_t maxVersion, const std::string &debugName)
    {
        return CreateRef<ConstantBuffer>(size, isVolatile, maxVersion, debugName);
    }
    
} // namespace ignite
