// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_VERTEX_BUFFER_HPP
#define IGN_VERTEX_BUFFER_HPP

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

#endif
