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

#pragma once

#include "types.hpp"

#include <string.h>

namespace ignite
{
    struct Buffer
    {
        uint8_t* Data = nullptr;
        uint64_t Size = 0;

        Buffer() = default;

        Buffer(uint64_t size)
        {
            Allocate(size);
        }

        Buffer(const void* data, uint64_t size)
            : Data((uint8_t*)data), Size(size)
        {
        }

        static Buffer Copy(Buffer other)
        {
            Buffer result(other.Size);
            memcpy(result.Data, other.Data, other.Size);
            return result;
        }

        void Allocate(uint64_t size)
        {
            Release();

            Data = static_cast<u8 *>(malloc(size));
            Size = size;
        }

        void Release()
        {
            free(Data);
            Data = nullptr;
            Size = 0;
        }

        template<typename T>
        T* As()
        {
            return static_cast<T *>(Data);
        }

        operator bool() const
        {
            return static_cast<bool>(Data);
        }
    };

    struct ScopedBuffer
    {
        ScopedBuffer(Buffer buffer)
            : m_Buffer(buffer)
        {
        }

        ScopedBuffer(uint64_t size)
            : m_Buffer(size)
        {
        }

        ~ScopedBuffer()
        {
            m_Buffer.Release();
        }

        uint8_t* Data() { return m_Buffer.Data; }
        uint8_t Size() { return static_cast<uint8_t>(m_Buffer.Size); }

        template<typename T>
        T* As()
        {
            return m_Buffer.As<T>();
        }

        operator bool() const { return m_Buffer; }

    private:
        Buffer m_Buffer;

    };
}
