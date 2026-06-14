// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_BATCH_DATA_HPP
#define IGN_BATCH_DATA_HPP

#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/texture.hpp"

#include <vector>

namespace ignite
{
    template<typename VertexType>
    struct BatchRender
    {
        uint32_t minCount = 256;
        uint32_t maxCount = minCount;
        uint32_t verticesPerObject = 4;
        uint32_t indicesPerObject = 6;
        uint32_t maxVertices = maxCount * verticesPerObject;
        uint32_t maxIndices = maxCount * indicesPerObject;
        uint32_t lowUsageFrames = 0;
        uint8_t textureSlotIndex = 1; // 0 for white texture
        uint32_t indexCount = 0;
        uint32_t count = 0;

        VertexType *vertexBufferBase = nullptr;
        VertexType *vertexBufferPtr = nullptr;
        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;
        std::vector<Ref<Texture>> textureSlots;

        ~BatchRender()
        {
            vertexBufferPtr = nullptr;
            delete[] vertexBufferBase;
        }
    };
}

#endif