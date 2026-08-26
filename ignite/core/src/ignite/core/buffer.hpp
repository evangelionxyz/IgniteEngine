// Copyright (c) 2025 Evangelion Manuhutu
#pragma once
#ifndef IGN_CORE_BUFFER_HPP
#define IGN_CORE_BUFFER_HPP

#include "ignite/core/base.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <span>
#include <vector>

namespace ignite
{
    class IBuffer
    {
    public:
        IBuffer() = default;

        IBuffer(const IBuffer& other)
        {
            if (!other.IsEmpty())
            {
                m_Data = other.m_Data;
            }
        }

        IBuffer(IBuffer&& other) noexcept
        {
            m_Data = std::move(other.m_Data);
        }

        IBuffer& operator=(const IBuffer& other)
        {
            if (this != &other)
            {
                Release();
                if (!other.IsEmpty())
                {
                    m_Data = other.m_Data;
                }
            }
            return *this;
        }

        IBuffer& operator=(IBuffer&& other) noexcept
        {
            if (this != &other)
            {
                Release();
                m_Data = std::move(other.m_Data);
            }
            return *this;
        }

        virtual ~IBuffer()
        {
            Release();
        }

        [[nodiscard]]
        virtual const uint8_t *Data() const { return m_Data.data(); }

        [[nodiscard]]
        virtual const uint8_t *Data() { return m_Data.data(); }

        [[nodiscard]]
        virtual const size_t Size() const { return m_Data.size(); }

        [[nodiscard]]
        static bool IsEmpty(IBuffer const *buffer)
        {
            return buffer == nullptr || buffer->Data() == nullptr || buffer->Size() == 0;
        }

        [[nodiscard]]
        inline bool IsEmpty() const
        {
            return m_Data.empty();
        }

        virtual void Allocate(size_t size)
        {
            Release();
            m_Data.resize(size);
        }

        virtual void Release()
        {
            if (!IsEmpty())
            {
                m_Data.clear();
                m_Data.shrink_to_fit(); // Forces deallocation of capacity
            }
        }

        template<typename T>
        T *As() { return (T *)Data(); }

        template<typename T>
        const T *As() const { return (T *)Data(); }

        operator bool() const { return !IsEmpty(); }

        const std::vector<uint8_t> &Get() const { return m_Data; }

    protected:
        std::vector<uint8_t> m_Data;
    };

    class Buffer : public IBuffer
    {
    public:
        Buffer() = default;

        Buffer(size_t size)
        {
            Allocate(size);
        }

		Buffer(const std::span<uint8_t> &inData)
		{
			if (!inData.empty())
			{
				Allocate(inData.size());
				std::memcpy(m_Data.data(), inData.data(), inData.size());
			}
		}

		Buffer(const std::vector<uint8_t> &inData)
		{
			if (!inData.empty())
			{
				Allocate(inData.size());
				std::memcpy(m_Data.data(), inData.data(), inData.size());
			}
		}

        Buffer(void *data, size_t size)
        {
            if (data && size > 0)
            {
                Allocate(size);
                std::memcpy(m_Data.data(), data, size);
            }
        }

        static Buffer Copy(uint8_t *data, size_t size)
        {
            if (!data || size == 0)
            {
                return { };
            }
            return { data, size };
        }

        static Buffer Copy(const std::span<uint8_t> &data)
        {
            if (data.empty())
                return { };
            return { data };
        }

        static Buffer Copy(const Buffer &other)
        {
            if (IBuffer::IsEmpty(&other))
            {
                return {};
            }

            return { other.m_Data };
        }
    };

    class ScopedBuffer
    {
    public:
        ScopedBuffer(Buffer buffer)
            : m_Buffer(buffer)
        {
        }

        ScopedBuffer(size_t size)
            : m_Buffer(size)
        {
        }

        ~ScopedBuffer()
        {
            m_Buffer.Release();
        }

        const uint8_t* Data() const { return m_Buffer.Data(); }
        const size_t Size() const { return m_Buffer.Size(); }

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

#endif
