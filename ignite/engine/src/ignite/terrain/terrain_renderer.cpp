// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "terrain_renderer.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    void TerrainRenderer::Init()
    {
    }

    void TerrainRenderer::RebuildMesh(nvrhi::ICommandList *cmd, TerrainComponent &comp, const glm::mat4 &worldTransform)
    {
        uint32_t res = std::max(2u, comp.resolution);
        uint32_t chunkCount = std::max(1u, comp.chunkCount);
        float step = comp.worldSize / static_cast<float>(res - 1);
        float halfSize = comp.worldSize * 0.5f;

        comp.chunks.clear();
        comp.chunks.reserve(static_cast<size_t>(chunkCount) * static_cast<size_t>(chunkCount));

        for (uint32_t cz = 0; cz < chunkCount; ++cz)
        {
            for (uint32_t cx = 0; cx < chunkCount; ++cx)
            {
                uint32_t startX = cx * (res - 1) / chunkCount;
                uint32_t endX = (cx == chunkCount - 1) ? (res - 1) : (cx + 1) * (res - 1) / chunkCount;
                uint32_t startZ = cz * (res - 1) / chunkCount;
                uint32_t endZ = (cz == chunkCount - 1) ? (res - 1) : (cz + 1) * (res - 1) / chunkCount;

                uint32_t chunkResX = endX - startX + 1;
                uint32_t chunkResZ = endZ - startZ + 1;

                std::vector<VertexMeshStatic> vertices;
                vertices.reserve(static_cast<size_t>(chunkResX) * static_cast<size_t>(chunkResZ));

                glm::vec3 minPos(std::numeric_limits<float>::max());
                glm::vec3 maxPos(std::numeric_limits<float>::lowest());

                for (uint32_t z = startZ; z <= endZ; ++z)
                {
                    for (uint32_t x = startX; x <= endX; ++x)
                    {
                        VertexMeshStatic vert;
                        float posX = static_cast<float>(x) * step - halfSize;
                        float posZ = static_cast<float>(z) * step - halfSize;
                        float posY = comp.data ? comp.data->GetHeight(x, z) * comp.maxHeight : 0.0f;

                        float hL = comp.data ? comp.data->GetHeight(x > 0 ? x - 1 : x, z) : 0.0f;
                        float hR = comp.data ? comp.data->GetHeight(x < res - 1 ? x + 1 : x, z) : 0.0f;
                        float hD = comp.data ? comp.data->GetHeight(x, z > 0 ? z - 1 : z) : 0.0f;
                        float hU = comp.data ? comp.data->GetHeight(x, z < res - 1 ? z + 1 : z) : 0.0f;

                        glm::vec3 normal = glm::normalize(glm::vec3((hL - hR) * comp.maxHeight, 2.0f * step, (hD - hU) * comp.maxHeight));
                        glm::vec3 tangent = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
                        glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

                        vert.position = glm::vec3(posX, posY, posZ);
                        vert.normal = normal;
                        vert.tangent = tangent;
                        vert.bitangent = bitangent;
                        vert.uv = glm::vec2(static_cast<float>(x) / static_cast<float>(res - 1), static_cast<float>(z) / static_cast<float>(res - 1));
                        vert.color = glm::vec4(1.0f);

                        minPos = glm::min(minPos, vert.position);
                        maxPos = glm::max(maxPos, vert.position);

                        vertices.push_back(vert);
                    }
                }

                std::vector<uint32_t> indices;
                indices.reserve(static_cast<size_t>(chunkResX - 1) * static_cast<size_t>(chunkResZ - 1) * 6);

                for (uint32_t lz = 0; lz < chunkResZ - 1; ++lz)
                {
                    for (uint32_t lx = 0; lx < chunkResX - 1; ++lx)
                    {
                        uint32_t i0 = lz * chunkResX + lx;
                        uint32_t i1 = lz * chunkResX + (lx + 1);
                        uint32_t i2 = (lz + 1) * chunkResX + (lx + 1);
                        uint32_t i3 = (lz + 1) * chunkResX + lx;

                        indices.push_back(i0);
                        indices.push_back(i2);
                        indices.push_back(i1);

                        indices.push_back(i0);
                        indices.push_back(i3);
                        indices.push_back(i2);
                    }
                }

                TerrainChunk chunk;
                chunk.gridX = cx;
                chunk.gridZ = cz;
                chunk.chunkResolution = chunkResX;
                chunk.lodLevel = 0;
                chunk.bounds.min = minPos;
                chunk.bounds.max = maxPos;
                chunk.primitive = MeshPrimitive<VertexMeshStatic>::Create(vertices, indices);

                if (cmd)
                {
                    chunk.primitive->WriteBuffer(cmd);
                }
                chunk.gpuBuffersDirty = false;

                comp.chunks.push_back(chunk);
            }
        }

        comp.gpuInitialized = true;
    }
}
