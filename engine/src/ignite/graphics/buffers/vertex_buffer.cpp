/* MIT License
*
* Copyright (c) 2025 Evangelion Manuhutu
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

        m_Handle = device->createBuffer(desc);
        LOG_ASSERT(m_Handle, "[Vertex Buffer] Failed to create handle!");
        if (m_Handle)
        {
            IGN_PROFILE_ALLOC_N(m_Handle.Get(), size, "GPU Vertex Buffer");
        }
    }

    VertexBuffer::~VertexBuffer()
    {
        if (m_Handle)
        {
            IGN_PROFILE_FREE_N(m_Handle.Get(), "GPU Vertex Buffer");
            m_Handle = nullptr;
        }
    }

	void VertexBuffer::SetData(nvrhi::ICommandList *cmd, const Buffer buffer, const size_t offset) const
    {
        IGN_PROFILE_SCOPE("VertexBuffer::SetData");
        cmd->writeBuffer(m_Handle, buffer.data, buffer.size, offset);
    }

    Ref<VertexBuffer> VertexBuffer::Create(size_t size, const std::string &debugName)
    {
        return CreateRef<VertexBuffer>(size, debugName);
    }
}
