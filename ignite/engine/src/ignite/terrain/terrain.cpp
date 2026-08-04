// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "terrain.hpp"
#include "ignite/core/logger.hpp"
#include <stb_image.h>
#include <glm/glm.hpp>
#include <algorithm>

namespace ignite
{
    void TerrainData::InitFlat(uint32_t res, float size, float maxH)
    {
        resolution = res;
        worldSize = size;
        maxHeight = maxH;
        heightmap.assign(static_cast<size_t>(resolution) * static_cast<size_t>(resolution), 0.0f);
        dirty = true;
    }

    float TerrainData::GetHeight(uint32_t x, uint32_t z) const
    {
        if (x >= resolution || z >= resolution)
        {
            return 0.0f;
        }
        return heightmap[static_cast<size_t>(z) * resolution + x];
    }

    void TerrainData::SetHeight(uint32_t x, uint32_t z, float val)
    {
        if (x >= resolution || z >= resolution)
        {
            return;
        }
        heightmap[static_cast<size_t>(z) * resolution + x] = val;
        dirty = true;
    }

    bool TerrainData::LoadHeightmap(const std::string &filepath)
    {
        int width = 0;
        int height = 0;
        int channels = 0;

        if (stbi_is_16_bit(filepath.c_str()))
        {
            uint16_t *pixels = stbi_load_16(filepath.c_str(), &width, &height, &channels, 1);
            if (!pixels)
            {
                LOG_ERROR("[TerrainData] Failed to load 16-bit heightmap: {}", filepath);
                return false;
            }

            resolution = static_cast<uint32_t>(width);
            heightmap.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
            for (int i = 0; i < width * height; ++i)
            {
                heightmap[i] = static_cast<float>(pixels[i]) / 65535.0f;
            }
            stbi_image_free(pixels);
        }
        else
        {
            uint8_t *pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 1);
            if (!pixels)
            {
                LOG_ERROR("[TerrainData] Failed to load heightmap: {}", filepath);
                return false;
            }

            resolution = static_cast<uint32_t>(width);
            heightmap.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
            for (int i = 0; i < width * height; ++i)
            {
                heightmap[i] = static_cast<float>(pixels[i]) / 255.0f;
            }
            stbi_image_free(pixels);
        }

        dirty = true;
        LOG_INFO("[TerrainData] Loaded heightmap ({}, {}) from {}", width, height, filepath);
        return true;
    }

    float TerrainData::SampleHeight(float worldX, float worldZ) const
    {
        if (resolution < 2 || worldSize <= 0.0f || heightmap.empty())
        {
            return 0.0f;
        }

        float halfSize = worldSize * 0.5f;
        float normX = (worldX + halfSize) / worldSize;
        float normZ = (worldZ + halfSize) / worldSize;

        normX = glm::clamp(normX, 0.0f, 1.0f);
        normZ = glm::clamp(normZ, 0.0f, 1.0f);

        float gridX = normX * static_cast<float>(resolution - 1);
        float gridZ = normZ * static_cast<float>(resolution - 1);

        uint32_t x0 = static_cast<uint32_t>(gridX);
        uint32_t z0 = static_cast<uint32_t>(gridZ);
        uint32_t x1 = std::min(x0 + 1, resolution - 1);
        uint32_t z1 = std::min(z0 + 1, resolution - 1);

        float tx = gridX - static_cast<float>(x0);
        float tz = gridZ - static_cast<float>(z0);

        float h00 = GetHeight(x0, z0);
        float h10 = GetHeight(x1, z0);
        float h01 = GetHeight(x0, z1);
        float h11 = GetHeight(x1, z1);

        float h0 = glm::mix(h00, h10, tx);
        float h1 = glm::mix(h01, h11, tx);

        return glm::mix(h0, h1, tz) * maxHeight;
    }

    glm::vec3 TerrainData::SampleNormal(float worldX, float worldZ) const
    {
        float delta = worldSize / static_cast<float>(std::max(1u, resolution - 1));
        float hL = SampleHeight(worldX - delta, worldZ);
        float hR = SampleHeight(worldX + delta, worldZ);
        float hD = SampleHeight(worldX, worldZ - delta);
        float hU = SampleHeight(worldX, worldZ + delta);

        glm::vec3 n(hL - hR, 2.0f * delta, hD - hU);
        return glm::normalize(n);
    }
}
