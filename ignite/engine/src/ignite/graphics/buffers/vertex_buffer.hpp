/* MIT License
*
* Copyright (c) 2026 Evangelion Manuhutu
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

#pragma once

#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

#include <nvrhi/nvrhi.h>

namespace ignite
{
    class VertexBuffer
    {
    public:
        VertexBuffer(const size_t size, const std::string &debugName = "Vertex Buffer");
        ~VertexBuffer();

        void SetData(nvrhi::ICommandList *cmd, void *data, size_t dataSize, size_t offset = 0) const;

        nvrhi::BufferHandle GetHandle() { return m_Handle; }
        operator nvrhi::BufferHandle() const { return m_Handle; }
        operator nvrhi::IBuffer *() const { return m_Handle; }

        size_t GetByteSize() const { return m_ByteSize; }

        static Ref<VertexBuffer> Create(const size_t size, const std::string &debugName = "Vertex Buffer");

    private:
        nvrhi::BufferHandle m_Handle;
        size_t m_ByteSize = 0;
    };
}
