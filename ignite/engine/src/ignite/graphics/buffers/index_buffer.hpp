// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_INDEX_BUFFER_HPP
#define IGN_INDEX_BUFFER_HPP

#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

#include <nvrhi/nvrhi.h>

namespace ignite
{
    class IndexBuffer
    {
    public:
        IndexBuffer(size_t size, const std::string &debugName = "Index Buffer");
        ~IndexBuffer();

        void SetData(nvrhi::ICommandList *cmd, void *data, size_t dataSize, size_t offset = 0) const;

        const uint32_t GetCount() { return m_Count; }
        size_t GetByteSize() const { return m_ByteSize; }

        nvrhi::BufferHandle GetHandle() { return m_Handle; }
        operator nvrhi::BufferHandle() const { return m_Handle; }
        operator nvrhi::IBuffer *() const { return m_Handle; }

        static Ref<IndexBuffer> Create(size_t size, const std::string &debugName = "Index Buffer");

    private:
        uint32_t m_Count = 0;
        size_t m_ByteSize = 0;
        nvrhi::BufferHandle m_Handle;
    };
}

#endif
