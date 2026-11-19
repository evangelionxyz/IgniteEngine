// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GI_ASSET_IO_HPP
#define IGN_GI_ASSET_IO_HPP

#include "ignite/core/base.hpp"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace ignite
{
    struct IrradianceVolumeData
    {
        static constexpr uint32_t kFormatVersion = 1;
        static constexpr uint32_t kFlagHasVisibilityAtlas = 1u << 0u;
        static constexpr uint32_t kFlagHasProbeOffsets = 1u << 1u;

        glm::ivec3 dims = glm::ivec3(0);
        glm::vec3 boundsMin = glm::vec3(0.0f);
        float spacing = 1.5f;
        uint32_t shOrder = 2;
        uint32_t flags = 0;

        uint32_t visOctRes = 8;
        uint32_t visAtlasCols = 0;
        uint32_t visAtlasRows = 0;
        uint64_t bakeHash = 0;

        // 7 x Texture3D<RGBA16_SFLOAT> packed via gi::PackSHL2To7RGBA and glm::packHalf1x16
        std::array<std::vector<uint16_t>, 7> shHalfRGBA;
        // Optional Texture3D<RGBA16_SFLOAT> probe relocation offset (xyz) + state (w: 0=invalid, 1=valid, 2=relocated)
        std::vector<uint16_t> probeOffsetsHalfRGBA;
        // Optional Texture2D<RG16_SFLOAT> octahedral visibility atlas (meanDist, meanDistSq)
        std::vector<uint16_t> visibilityAtlasHalfRG;

        uint32_t GetProbeCount() const
        {
            if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0)
            {
                return 0;
            }
            return static_cast<uint32_t>(dims.x) * static_cast<uint32_t>(dims.y) * static_cast<uint32_t>(dims.z);
        }

        uint32_t GetVisibilityAtlasWidth() const
        {
            return visAtlasCols * (visOctRes + 2u);
        }

        uint32_t GetVisibilityAtlasHeight() const
        {
            return visAtlasRows * (visOctRes + 2u);
        }
    };

    struct ReflectionProbeData
    {
        static constexpr uint32_t kFormatVersion = 1;

        uint32_t resolution = 128;
        uint32_t mipCount = 6;
        glm::vec3 capturePos = glm::vec3(0.0f);
        glm::vec3 boxMin = glm::vec3(-5.0f);
        glm::vec3 boxMax = glm::vec3(5.0f);
        std::array<glm::vec4, 4> captureSH = {};
        uint64_t bakeHash = 0;

        // 6 * mipCount subresources of RGBA16_SFLOAT bytes in (face * mipCount + mip) order
        std::vector<std::vector<uint8_t>> mipFaceBytes;
    };

    namespace gi_io
    {
        IGN_API bool SaveIrradianceVolume(const std::filesystem::path &path, const IrradianceVolumeData &data);
        IGN_API bool LoadIrradianceVolume(const std::filesystem::path &path, IrradianceVolumeData &outData);

        IGN_API bool SaveReflectionProbe(const std::filesystem::path &path, const ReflectionProbeData &data);
        IGN_API bool LoadReflectionProbe(const std::filesystem::path &path, ReflectionProbeData &outData);
    }
}

#endif

