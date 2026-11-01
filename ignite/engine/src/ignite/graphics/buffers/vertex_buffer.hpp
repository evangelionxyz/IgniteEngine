// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_VERTEX_BUFFER_HPP
#define IGN_VERTEX_BUFFER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

#include <NRI.h>

namespace ignite
{
    class VertexBuffer
    {
    public:
        VertexBuffer(const size_t size, const std::string &debugName = "Vertex Buffer");
        ~VertexBuffer();

        void SetData(const void *data, size_t dataSize, size_t offset = 0) const;
        void SetData(const Buffer &buffer, size_t offset = 0) const { SetData(buffer.Data(), buffer.Size(), offset); }
        void SetData(nri::CommandBuffer *cmd, const void *data, size_t dataSize, size_t offset = 0) const { (void)cmd; SetData(data, dataSize, offset); }

        nri::Buffer *GetHandle() const { return m_Handle; }
        operator nri::Buffer *() const { return m_Handle; }

        size_t GetByteSize() const { return m_ByteSize; }

        static Ref<VertexBuffer> Create(const size_t size, const std::string &debugName = "Vertex Buffer");

    private:
        nri::Buffer *m_Handle = nullptr;
        size_t m_ByteSize = 0;
    };
}

#endif
