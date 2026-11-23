// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "gi_asset_io.hpp"
#include "ignite/core/logger.hpp"

#include <cstring>
#include <fstream>

namespace ignite::gi_io
{
    namespace
    {
        constexpr char kIGIMagic[4] = { 'I', 'G', 'G', 'I' };
        constexpr char kIGCubeMagic[4] = { 'I', 'G', 'C', 'B' };

        struct IGIHeader
        {
            char magic[4] = {};
            uint32_t version = 0;
            int32_t dims[3] = { 0, 0, 0 };
            float boundsMin[3] = { 0.0f, 0.0f, 0.0f };
            float spacing = 1.5f;
            uint32_t shOrder = 2;
            uint32_t flags = 0;
            uint32_t visOctRes = 8;
            uint32_t visAtlasCols = 0;
            uint32_t visAtlasRows = 0;
            uint32_t _pad = 0;
            uint64_t bakeHash = 0;
        };

        struct IGCubeHeader
        {
            char magic[4] = {};
            uint32_t version = 0;
            uint32_t resolution = 0;
            uint32_t mipCount = 0;
            float capturePos[3] = { 0.0f, 0.0f, 0.0f };
            float boxMin[3] = { 0.0f, 0.0f, 0.0f };
            float boxMax[3] = { 0.0f, 0.0f, 0.0f };
            uint32_t _pad = 0;
            float captureSH[4][4] = {};
            uint64_t bakeHash = 0;
        };

        template<typename T>
        bool WriteExact(std::ofstream &out, const T *ptr, size_t count)
        {
            if (count == 0)
            {
                return true;
            }
            out.write(reinterpret_cast<const char *>(ptr), static_cast<std::streamsize>(count * sizeof(T)));
            return static_cast<bool>(out);
        }

        template<typename T>
        bool ReadExact(std::ifstream &in, T *ptr, size_t count)
        {
            if (count == 0)
            {
                return true;
            }
            in.read(reinterpret_cast<char *>(ptr), static_cast<std::streamsize>(count * sizeof(T)));
            return static_cast<bool>(in) && static_cast<size_t>(in.gcount()) == count * sizeof(T);
        }
    }

    bool SaveIrradianceVolume(const std::filesystem::path &path, const IrradianceVolumeData &data)
    {
        const uint32_t probeCount = data.GetProbeCount();
        if (probeCount == 0 || path.empty())
        {
            return false;
        }

        const size_t expectedSHSize = static_cast<size_t>(probeCount) * 4u;
        for (int t = 0; t < 7; ++t)
        {
            if (data.shHalfRGBA[t].size() != expectedSHSize)
            {
                return false;
            }
        }

        std::error_code ec;
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            LOG_ERROR("[GI IO] Failed to open '{}' for writing", path.string());
            return false;
        }

        const bool hasOffsets = (data.probeOffsetsHalfRGBA.size() == expectedSHSize);
        const size_t visAtlasTexels = static_cast<size_t>(data.GetVisibilityAtlasWidth()) * static_cast<size_t>(data.GetVisibilityAtlasHeight());
        const bool hasVis = (visAtlasTexels > 0 && data.visibilityAtlasHalfRG.size() == visAtlasTexels * 2u);
        const bool hasSkyVis = (data.skyVisibilityHalfRGBA.size() == expectedSHSize);

        IGIHeader hdr{};
        std::memcpy(hdr.magic, kIGIMagic, sizeof(kIGIMagic));
        hdr.version = IrradianceVolumeData::kFormatVersion;
        hdr.dims[0] = data.dims.x;
        hdr.dims[1] = data.dims.y;
        hdr.dims[2] = data.dims.z;
        hdr.boundsMin[0] = data.boundsMin.x;
        hdr.boundsMin[1] = data.boundsMin.y;
        hdr.boundsMin[2] = data.boundsMin.z;
        hdr.spacing = data.spacing;
        hdr.shOrder = data.shOrder;
        hdr.flags = (hasVis ? IrradianceVolumeData::kFlagHasVisibilityAtlas : 0u)
                  | (hasOffsets ? IrradianceVolumeData::kFlagHasProbeOffsets : 0u)
                  | (hasSkyVis ? IrradianceVolumeData::kFlagHasSkyVisibility : 0u);
        hdr.visOctRes = data.visOctRes;
        hdr.visAtlasCols = hasVis ? data.visAtlasCols : 0u;
        hdr.visAtlasRows = hasVis ? data.visAtlasRows : 0u;
        hdr.bakeHash = data.bakeHash;

        if (!WriteExact(out, &hdr, 1))
        {
            return false;
        }

        for (int t = 0; t < 7; ++t)
        {
            if (!WriteExact(out, data.shHalfRGBA[t].data(), expectedSHSize))
            {
                return false;
            }
        }

        if (hasOffsets && !WriteExact(out, data.probeOffsetsHalfRGBA.data(), expectedSHSize))
        {
            return false;
        }

        if (hasVis && !WriteExact(out, data.visibilityAtlasHalfRG.data(), visAtlasTexels * 2u))
        {
            return false;
        }

        if (hasSkyVis && !WriteExact(out, data.skyVisibilityHalfRGBA.data(), expectedSHSize))
        {
            return false;
        }

        return true;
    }

    bool LoadIrradianceVolume(const std::filesystem::path &path, IrradianceVolumeData &outData)
    {
        outData = IrradianceVolumeData{};
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            return false;
        }

        IGIHeader hdr{};
        if (!ReadExact(in, &hdr, 1))
        {
            return false;
        }

        if (std::memcmp(hdr.magic, kIGIMagic, sizeof(kIGIMagic)) != 0 || hdr.version != IrradianceVolumeData::kFormatVersion)
        {
            return false;
        }

        if (hdr.dims[0] <= 0 || hdr.dims[1] <= 0 || hdr.dims[2] <= 0 || hdr.dims[0] > 256 || hdr.dims[1] > 256 || hdr.dims[2] > 256)
        {
            return false;
        }

        outData.dims = glm::ivec3(hdr.dims[0], hdr.dims[1], hdr.dims[2]);
        outData.boundsMin = glm::vec3(hdr.boundsMin[0], hdr.boundsMin[1], hdr.boundsMin[2]);
        outData.spacing = hdr.spacing;
        outData.shOrder = hdr.shOrder;
        outData.flags = hdr.flags;
        outData.visOctRes = hdr.visOctRes;
        outData.visAtlasCols = hdr.visAtlasCols;
        outData.visAtlasRows = hdr.visAtlasRows;
        outData.bakeHash = hdr.bakeHash;

        const size_t probeCount = static_cast<size_t>(outData.GetProbeCount());
        const size_t expectedSHSize = probeCount * 4u;
        for (int t = 0; t < 7; ++t)
        {
            outData.shHalfRGBA[t].resize(expectedSHSize);
            if (!ReadExact(in, outData.shHalfRGBA[t].data(), expectedSHSize))
            {
                outData = IrradianceVolumeData{};
                return false;
            }
        }

        if ((hdr.flags & IrradianceVolumeData::kFlagHasProbeOffsets) != 0u)
        {
            outData.probeOffsetsHalfRGBA.resize(expectedSHSize);
            if (!ReadExact(in, outData.probeOffsetsHalfRGBA.data(), expectedSHSize))
            {
                outData = IrradianceVolumeData{};
                return false;
            }
        }

        if ((hdr.flags & IrradianceVolumeData::kFlagHasVisibilityAtlas) != 0u)
        {
            const size_t visAtlasTexels = static_cast<size_t>(outData.GetVisibilityAtlasWidth()) * static_cast<size_t>(outData.GetVisibilityAtlasHeight());
            if (visAtlasTexels == 0)
            {
                outData = IrradianceVolumeData{};
                return false;
            }
            outData.visibilityAtlasHalfRG.resize(visAtlasTexels * 2u);
            if (!ReadExact(in, outData.visibilityAtlasHalfRG.data(), visAtlasTexels * 2u))
            {
                outData = IrradianceVolumeData{};
                return false;
            }
        }

        if ((hdr.flags & IrradianceVolumeData::kFlagHasSkyVisibility) != 0u)
        {
            outData.skyVisibilityHalfRGBA.resize(expectedSHSize);
            if (!ReadExact(in, outData.skyVisibilityHalfRGBA.data(), expectedSHSize))
            {
                outData = IrradianceVolumeData{};
                return false;
            }
        }

        return true;
    }

    bool SaveReflectionProbe(const std::filesystem::path &path, const ReflectionProbeData &data)
    {
        if (data.resolution == 0 || data.mipCount == 0 || data.mipFaceBytes.size() != 6u * data.mipCount || path.empty())
        {
            return false;
        }

        std::error_code ec;
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return false;
        }

        IGCubeHeader hdr{};
        std::memcpy(hdr.magic, kIGCubeMagic, sizeof(kIGCubeMagic));
        hdr.version = ReflectionProbeData::kFormatVersion;
        hdr.resolution = data.resolution;
        hdr.mipCount = data.mipCount;
        hdr.capturePos[0] = data.capturePos.x; hdr.capturePos[1] = data.capturePos.y; hdr.capturePos[2] = data.capturePos.z;
        hdr.boxMin[0] = data.boxMin.x; hdr.boxMin[1] = data.boxMin.y; hdr.boxMin[2] = data.boxMin.z;
        hdr.boxMax[0] = data.boxMax.x; hdr.boxMax[1] = data.boxMax.y; hdr.boxMax[2] = data.boxMax.z;
        for (int i = 0; i < 4; ++i)
        {
            hdr.captureSH[i][0] = data.captureSH[i].x;
            hdr.captureSH[i][1] = data.captureSH[i].y;
            hdr.captureSH[i][2] = data.captureSH[i].z;
            hdr.captureSH[i][3] = data.captureSH[i].w;
        }
        hdr.bakeHash = data.bakeHash;

        if (!WriteExact(out, &hdr, 1))
        {
            return false;
        }

        for (uint32_t face = 0; face < 6u; ++face)
        {
            for (uint32_t mip = 0; mip < data.mipCount; ++mip)
            {
                const uint32_t mipSize = std::max(1u, data.resolution >> mip);
                const size_t expectedBytes = static_cast<size_t>(mipSize) * mipSize * 4u * sizeof(uint16_t);
                const auto &sub = data.mipFaceBytes[static_cast<size_t>(face) * data.mipCount + mip];
                if (sub.size() != expectedBytes || !WriteExact(out, sub.data(), sub.size()))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool LoadReflectionProbe(const std::filesystem::path &path, ReflectionProbeData &outData)
    {
        outData = ReflectionProbeData{};
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            return false;
        }

        IGCubeHeader hdr{};
        if (!ReadExact(in, &hdr, 1))
        {
            return false;
        }

        if (std::memcmp(hdr.magic, kIGCubeMagic, sizeof(kIGCubeMagic)) != 0 || hdr.version != ReflectionProbeData::kFormatVersion)
        {
            return false;
        }
        if (hdr.resolution == 0 || hdr.resolution > 4096 || hdr.mipCount == 0 || hdr.mipCount > 13)
        {
            return false;
        }

        outData.resolution = hdr.resolution;
        outData.mipCount = hdr.mipCount;
        outData.capturePos = glm::vec3(hdr.capturePos[0], hdr.capturePos[1], hdr.capturePos[2]);
        outData.boxMin = glm::vec3(hdr.boxMin[0], hdr.boxMin[1], hdr.boxMin[2]);
        outData.boxMax = glm::vec3(hdr.boxMax[0], hdr.boxMax[1], hdr.boxMax[2]);
        for (int i = 0; i < 4; ++i)
        {
            outData.captureSH[i] = glm::vec4(hdr.captureSH[i][0], hdr.captureSH[i][1], hdr.captureSH[i][2], hdr.captureSH[i][3]);
        }
        outData.bakeHash = hdr.bakeHash;

        outData.mipFaceBytes.resize(static_cast<size_t>(6u) * outData.mipCount);
        for (uint32_t face = 0; face < 6u; ++face)
        {
            for (uint32_t mip = 0; mip < outData.mipCount; ++mip)
            {
                const uint32_t mipSize = std::max(1u, outData.resolution >> mip);
                const size_t expectedBytes = static_cast<size_t>(mipSize) * mipSize * 4u * sizeof(uint16_t);
                auto &sub = outData.mipFaceBytes[static_cast<size_t>(face) * outData.mipCount + mip];
                sub.resize(expectedBytes);
                if (!ReadExact(in, sub.data(), expectedBytes))
                {
                    outData = ReflectionProbeData{};
                    return false;
                }
            }
        }

        return true;
    }
}

