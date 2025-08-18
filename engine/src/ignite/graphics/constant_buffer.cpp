#include "constant_buffer.hpp"

#include "ignite/core/application.hpp"

namespace ignite
{
    ConstantBuffer::ConstantBuffer(const size_t size, bool isVolatile, const uint32_t maxVersion, const std::string &debugName)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

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
    }
    
    void ConstantBuffer::SetData(nvrhi::ICommandList *commandList, Buffer buffer, const size_t offset)
    {
        commandList->writeBuffer(m_Handle, buffer.data, buffer.size, offset);
    }

    void ConstantBuffer::SetData(Buffer buffer, const size_t offset)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        const nvrhi::CommandListHandle commandList = device->createCommandList();

        commandList->open();
        commandList->writeBuffer(m_Handle, buffer.data, buffer.size, offset);

        commandList->close();
        device->executeCommandList(commandList);
    }

    Ref<ConstantBuffer> ConstantBuffer::Create(const size_t size, bool isVolatile, const uint32_t maxVersion, const std::string &debugName)
    {
        return CreateRef<ConstantBuffer>(size, isVolatile, maxVersion, debugName);
    }
    
} // namespace ignite
