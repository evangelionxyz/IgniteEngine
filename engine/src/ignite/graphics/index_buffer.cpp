/* MIT License
*
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "index_buffer.hpp"

#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    IndexBuffer::IndexBuffer(size_t size)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        nvrhi::BufferDesc desc;
        desc.byteSize = size;
        desc.isIndexBuffer = true;
        desc.keepInitialState = true;
        desc.initialState = nvrhi::ResourceStates::IndexBuffer;

        m_Handle = device->createBuffer(desc);
        LOG_ASSERT(m_Handle, "[Index Buffer] Failed to create handle!");
    }

    void IndexBuffer::SetData(Buffer buffer, size_t offset) const
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        const nvrhi::CommandListHandle commandList = device->createCommandList();

        commandList->open();
        commandList->writeBuffer(m_Handle, buffer.data, buffer.size, offset);

        commandList->close();
        device->executeCommandList(commandList);
    }

    Ref<IndexBuffer> IndexBuffer::Create(size_t size)
    {
        return CreateRef<IndexBuffer>(size);
    }
}
