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

#pragma once

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string.h>

#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    struct Buffer
    {
        uint8_t* data = nullptr;
        uint64_t size = 0;

        Buffer() = default;
        Buffer(uint64_t size)
        {
            Allocate(size);
        }

        Buffer(const std::vector<uint8_t> &inData)
        {
            Allocate(inData.size());
            memcpy(data, inData.data(), size);
        }

        Buffer(void* data, uint64_t size)
            : data(static_cast<uint8_t *>(data)), size(size)
        {
        }

        static Buffer Copy(uint8_t *inData, uint64_t size)
        {
            if (!inData || size == 0)
            {
                return {};
            }

            Buffer result(size);
            memcpy(result.data, inData, size);
            return result;
        }

        static Buffer Copy(const std::vector<uint8_t> &data)
        {
            if (data.empty())
                return {};

            Buffer result(data.size());
            memcpy(result.data, data.data(), data.size());
            return result;
        }

        static Buffer Copy(const Buffer &other)
        {
            if (!other.data || other.size == 0)
            {
                return {};
            }

            Buffer result(other.size);
            memcpy(result.data, other.data, other.size);
            return result;
        }

        void Allocate(uint64_t size)
        {
            Release();

            data = static_cast<uint8_t *>(std::malloc(size));
            this->size = size;
            if (data && size > 0)
            {
                IGN_PROFILE_ALLOC_N(data, size, "CPU Buffer");
            }
        }

        void Release()
        {
            if (data != nullptr)
            {
                IGN_PROFILE_FREE_N(data, "CPU Buffer");
                std::free(data);
                data = nullptr;
                size = 0;
            }
        }

        template<typename T>
        T* As()
        {
            return static_cast<T *>(data);
        }

        operator bool() const
        {
            return static_cast<bool>(data);
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

        uint8_t* Data() { return m_Buffer.data; }
        uint8_t Size() { return static_cast<uint8_t>(m_Buffer.size); }

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
