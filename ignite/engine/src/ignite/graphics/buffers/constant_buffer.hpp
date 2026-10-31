// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_CONSTANT_BUFFER_HPP
#define IGN_CONSTANT_BUFFER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

#include <NRI.h>

namespace ignite
{
    class ConstantBuffer
    {
    public:
        ConstantBuffer(const size_t size, bool isVolatile = true, const uint32_t maxVersion = 16, const std::string &debugName = "Constant Buffer");
        ~ConstantBuffer();

        void SetData(const void *data, size_t dataSize, const size_t offset = 0);
        void SetData(const Buffer &buffer, const size_t offset = 0) { SetData(buffer.Data(), buffer.Size(), offset); }
        void SetData(nri::CommandBuffer *cmd, const void *data, size_t dataSize, const size_t offset = 0) { (void)cmd; SetData(data, dataSize, offset); }

        nri::Buffer *GetHandle() const { return m_Handle; }
        nri::Descriptor *GetDescriptor() const { return m_Descriptor; }
        operator nri::Buffer *() const { return m_Handle; }

        size_t GetByteSize() const { return m_ByteSize; }

        static Ref<ConstantBuffer> Create(const size_t size, bool isVolatile = true, const uint32_t maxVersion = 16, const std::string &debugName = "Constant Buffer");

    private:
        nri::Buffer *m_Handle = nullptr;
        nri::Descriptor *m_Descriptor = nullptr;
        size_t m_ByteSize = 0;
    };
}

#endif
