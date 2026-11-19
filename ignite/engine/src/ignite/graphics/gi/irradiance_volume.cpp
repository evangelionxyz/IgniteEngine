// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "irradiance_volume.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/core/logger.hpp"

#include <glm/gtc/packing.hpp>
#include <algorithm>

namespace ignite
{
    namespace
    {
        Ref<Texture> CreateAndUploadTexture3D(int32_t w, int32_t h, int32_t d, nri::Format format,
            const void *data, uint32_t bytesPerPixel, const std::string &debugName)
        {
            if (w <= 0 || h <= 0 || d <= 0 || !data)
            {
                return nullptr;
            }

            TextureCreateInfo info;
            info.width = static_cast<uint32_t>(w);
            info.height = static_cast<uint32_t>(h);
            info.depth = static_cast<uint32_t>(d);
            info.mipLevels = 1;
            info.arraySize = 1;
            info.sampleCount = 1;
            info.dimension = nri::TextureType::TEXTURE_3D;
            info.format = format;
            info.isCubeMap = false;
            info.isRenderTarget = false;
            info.isUAV = false;
            info.bindless = true;
            info.forceBindless = true;
            info.samplerLinearFiltering = true;
            info.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
            info.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
            info.samplerAddressW = nri::AddressMode::CLAMP_TO_EDGE;

            Ref<Texture> tex = Texture::Create(info, debugName);
            if (!tex)
            {
                return nullptr;
            }

            const uint32_t rowPitch = info.width * bytesPerPixel;
            const uint32_t slicePitch = rowPitch * info.height;
            if (!tex->UploadRawData(data, rowPitch, slicePitch, 0, 0))
            {
                return nullptr;
            }
            return tex;
        }
    }

    bool IrradianceVolume::UploadFromData(const IrradianceVolumeData &data)
    {
        const uint32_t probeCount = data.GetProbeCount();
        if (probeCount == 0)
        {
            return false;
        }

        const size_t expectedHalfCount = static_cast<size_t>(probeCount) * 4u;
        for (int t = 0; t < 7; ++t)
        {
            if (data.shHalfRGBA[t].size() != expectedHalfCount)
            {
                return false;
            }
        }

        m_Data = data;
        m_OffsetTexture.reset();
        m_VisibilityAtlas.reset();

        constexpr uint32_t kRGBA16BytesPerPixel = 8u;
        for (int t = 0; t < 7; ++t)
        {
            m_SHTextures[t] = CreateAndUploadTexture3D(
                data.dims.x, data.dims.y, data.dims.z,
                nri::Format::RGBA16_SFLOAT,
                data.shHalfRGBA[t].data(),
                kRGBA16BytesPerPixel,
                "[GI IrradianceVolume SH " + std::to_string(t) + "]");
            if (!m_SHTextures[t])
            {
                return false;
            }
        }

        if (data.probeOffsetsHalfRGBA.size() == expectedHalfCount)
        {
            m_OffsetTexture = CreateAndUploadTexture3D(
                data.dims.x, data.dims.y, data.dims.z,
                nri::Format::RGBA16_SFLOAT,
                data.probeOffsetsHalfRGBA.data(),
                kRGBA16BytesPerPixel,
                "[GI IrradianceVolume Offsets]");
        }

        const uint32_t visW = data.GetVisibilityAtlasWidth();
        const uint32_t visH = data.GetVisibilityAtlasHeight();
        if (visW > 0 && visH > 0 && data.visibilityAtlasHalfRG.size() == static_cast<size_t>(visW) * visH * 2u)
        {
            TextureCreateInfo visInfo;
            visInfo.width = visW;
            visInfo.height = visH;
            visInfo.depth = 1;
            visInfo.mipLevels = 1;
            visInfo.arraySize = 1;
            visInfo.sampleCount = 1;
            visInfo.dimension = nri::TextureType::TEXTURE_2D;
            visInfo.format = nri::Format::RG16_SFLOAT;
            visInfo.isCubeMap = false;
            visInfo.isRenderTarget = false;
            visInfo.isUAV = false;
            visInfo.bindless = true;
            visInfo.forceBindless = true;
            visInfo.samplerLinearFiltering = true;
            visInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
            visInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;

            m_VisibilityAtlas = Texture::Create(visInfo, "[GI IrradianceVolume VisibilityAtlas]");
            if (m_VisibilityAtlas)
            {
                const uint32_t rowPitch = visW * 4u; // RG16_SFLOAT = 4 bytes per pixel
                m_VisibilityAtlas->UploadRawData(data.visibilityAtlasHalfRG.data(), rowPitch, rowPitch * visH, 0, 0);
            }
        }

        BindlessSystem::FlushPendingWrites();
        return IsValid();
    }

    bool IrradianceVolume::LoadFromFile(const std::filesystem::path &path)
    {
        IrradianceVolumeData loaded;
        if (!gi_io::LoadIrradianceVolume(path, loaded))
        {
            return false;
        }
        return UploadFromData(loaded);
    }

    bool IrradianceVolume::IsValid() const
    {
        if (m_Data.GetProbeCount() == 0)
        {
            return false;
        }
        for (const auto &tex : m_SHTextures)
        {
            if (!tex || tex->GetBindlessIndex() == UINT32_MAX)
            {
                return false;
            }
        }
        return true;
    }

    void IrradianceVolume::FillGIGPUData(GI_GPUData &outGI, const IrradianceVolumeComponent *comp) const
    {
        if (!IsValid())
        {
            outGI.biasAndFade.w = 0.0f;
            outGI.shTextureIndices0 = glm::ivec4(-1);
            outGI.shTextureIndices1 = glm::ivec4(-1);
            outGI.visibilityParams = glm::ivec4(-1, 8, 0, 0);
            return;
        }

        const float intensity = comp ? comp->intensity : 1.0f;
        const float normalBias = comp ? comp->normalBias : 0.25f;
        const float viewBias = comp ? comp->viewBias : 0.1f;
        const float edgeFade = comp ? comp->edgeFade : 1.0f;
        const int32_t runtimeMode = comp ? comp->runtimeMode : 1;

        const glm::vec3 gridSize = glm::vec3(
            static_cast<float>(std::max(1, m_Data.dims.x - 1)),
            static_cast<float>(std::max(1, m_Data.dims.y - 1)),
            static_cast<float>(std::max(1, m_Data.dims.z - 1))) * m_Data.spacing;

        outGI.gridMinAndIntensity = glm::vec4(m_Data.boundsMin, intensity);
        outGI.gridSizeAndSpacing = glm::vec4(gridSize, m_Data.spacing);
        outGI.gridDimsAndOrder = glm::ivec4(m_Data.dims.x, m_Data.dims.y, m_Data.dims.z, static_cast<int32_t>(m_Data.shOrder));
        outGI.biasAndFade = glm::vec4(normalBias, viewBias, edgeFade, 1.0f);
        outGI.giMode = runtimeMode;

        outGI.shTextureIndices0 = glm::ivec4(
            static_cast<int32_t>(m_SHTextures[0]->GetBindlessIndex()),
            static_cast<int32_t>(m_SHTextures[1]->GetBindlessIndex()),
            static_cast<int32_t>(m_SHTextures[2]->GetBindlessIndex()),
            static_cast<int32_t>(m_SHTextures[3]->GetBindlessIndex()));

        const int32_t offsetIdx = (m_OffsetTexture && m_OffsetTexture->GetBindlessIndex() != UINT32_MAX)
            ? static_cast<int32_t>(m_OffsetTexture->GetBindlessIndex())
            : -1;
        outGI.shTextureIndices1 = glm::ivec4(
            static_cast<int32_t>(m_SHTextures[4]->GetBindlessIndex()),
            static_cast<int32_t>(m_SHTextures[5]->GetBindlessIndex()),
            static_cast<int32_t>(m_SHTextures[6]->GetBindlessIndex()),
            offsetIdx);

        const int32_t visIdx = (m_VisibilityAtlas && m_VisibilityAtlas->GetBindlessIndex() != UINT32_MAX)
            ? static_cast<int32_t>(m_VisibilityAtlas->GetBindlessIndex())
            : -1;
        outGI.visibilityParams = glm::ivec4(
            visIdx,
            static_cast<int32_t>(m_Data.visOctRes),
            static_cast<int32_t>(m_Data.visAtlasCols),
            static_cast<int32_t>(m_Data.visAtlasRows));
    }

    std::vector<gi::DebugProbeGPU> IrradianceVolume::BuildDebugProbes(float radius) const
    {
        std::vector<gi::DebugProbeGPU> result;
        const uint32_t probeCount = m_Data.GetProbeCount();
        if (probeCount == 0)
        {
            return result;
        }

        const size_t expectedHalfCount = static_cast<size_t>(probeCount) * 4u;
        for (int t = 0; t < 7; ++t)
        {
            if (m_Data.shHalfRGBA[t].size() != expectedHalfCount)
            {
                return result;
            }
        }

        const bool hasOffsets = (m_Data.probeOffsetsHalfRGBA.size() == expectedHalfCount);
        result.reserve(probeCount);

        for (int32_t iz = 0; iz < m_Data.dims.z; ++iz)
        {
            for (int32_t iy = 0; iy < m_Data.dims.y; ++iy)
            {
                for (int32_t ix = 0; ix < m_Data.dims.x; ++ix)
                {
                    const size_t probeIdx = (static_cast<size_t>(iz) * m_Data.dims.y + iy) * m_Data.dims.x + ix;
                    const size_t base = probeIdx * 4u;

                    glm::vec4 packed[7];
                    for (int t = 0; t < 7; ++t)
                    {
                        packed[t] = glm::vec4(
                            glm::unpackHalf1x16(m_Data.shHalfRGBA[t][base + 0u]),
                            glm::unpackHalf1x16(m_Data.shHalfRGBA[t][base + 1u]),
                            glm::unpackHalf1x16(m_Data.shHalfRGBA[t][base + 2u]),
                            glm::unpackHalf1x16(m_Data.shHalfRGBA[t][base + 3u]));
                    }

                    gi::SHL2 sh{};
                    float validity = 1.0f;
                    gi::UnpackSHL2From7RGBA(packed, sh, validity);

                    glm::vec3 pos = m_Data.boundsMin + glm::vec3(ix, iy, iz) * m_Data.spacing;
                    float state = (validity > 0.5f) ? 1.0f : 0.0f;
                    if (hasOffsets)
                    {
                        const glm::vec3 off(
                            glm::unpackHalf1x16(m_Data.probeOffsetsHalfRGBA[base + 0u]),
                            glm::unpackHalf1x16(m_Data.probeOffsetsHalfRGBA[base + 1u]),
                            glm::unpackHalf1x16(m_Data.probeOffsetsHalfRGBA[base + 2u]));
                        pos += off;
                        state = glm::unpackHalf1x16(m_Data.probeOffsetsHalfRGBA[base + 3u]);
                    }

                    gi::DebugProbeGPU probe{};
                    probe.posAndRadius = glm::vec4(pos, radius);
                    for (int i = 0; i < 9; ++i)
                    {
                        probe.sh[i] = glm::vec4(sh.c[i], 0.0f);
                    }
                    probe.stateAndFlags = glm::vec4(state, 0.0f, 0.0f, 0.0f);
                    result.push_back(probe);
                }
            }
        }

        return result;
    }
}

