// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_INDEX_BUFFER_HPP
#define IGN_INDEX_BUFFER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

#include <NRI.h>

namespace ignite
{
    class IndexBuffer
    {
    public:
        IndexBuffer(size_t size, const std::string &debugName = "Index Buffer");
        ~IndexBuffer();

        void SetData(const void *data, size_t dataSize, size_t offset = 0) const;
        void SetData(const Buffer &buffer, size_t offset = 0) const { SetData(buffer.Data(), buffer.Size(), offset); }
        void SetData(nri::CommandBuffer *cmd, const void *data, size_t dataSize, size_t offset = 0) const { (void)cmd; SetData(data, dataSize, offset); }

        uint32_t GetCount() const { return m_Count; }
        size_t GetByteSize() const { return m_ByteSize; }

        nri::Buffer *GetHandle() const { return m_Handle; }
        operator nri::Buffer *() const { return m_Handle; }

        static Ref<IndexBuffer> Create(size_t size, const std::string &debugName = "Index Buffer");

    private:
        uint32_t m_Count = 0;
        size_t m_ByteSize = 0;
        nri::Buffer *m_Handle = nullptr;
    };
}

#endif
