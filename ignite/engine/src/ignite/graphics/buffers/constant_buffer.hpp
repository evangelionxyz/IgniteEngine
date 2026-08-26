// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_CONSTANT_BUFFER_HPP
#define IGN_CONSTANT_BUFFER_HPP

#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

#include <nvrhi/nvrhi.h>

namespace ignite
{
    class ConstantBuffer
    {
    public:
        ConstantBuffer(const size_t size, bool isVolatile = true, const uint32_t maxVersion = 16, const std::string &debugName = "Constant Buffer");
        ~ConstantBuffer();

        void SetData(nvrhi::ICommandList *cmd, void *data, size_t dataSize, const size_t offset = 0);

        nvrhi::BufferHandle GetHandle() { return m_Handle; }
        operator nvrhi::BufferHandle() const { return m_Handle; }
        operator nvrhi::IBuffer *() const { return m_Handle; }

        size_t GetByteSize() const { return m_ByteSize; }

        static Ref<ConstantBuffer> Create(const size_t size, bool isVolatile = true, const uint32_t maxVersion = 16, const std::string &debugName = "Constant Buffer");
    private:
        nvrhi::BufferHandle m_Handle;
        size_t m_ByteSize = 0;
    };
}

#endif
