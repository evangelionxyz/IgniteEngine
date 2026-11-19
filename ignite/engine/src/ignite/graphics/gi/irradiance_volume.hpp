// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_IRRADIANCE_VOLUME_HPP
#define IGN_IRRADIANCE_VOLUME_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/gi/gi_asset_io.hpp"
#include "ignite/graphics/gi/sh.hpp"

#include <array>
#include <filesystem>
#include <vector>

namespace ignite
{
    class IrradianceVolumeComponent;

    // Runtime GPU representation of a baked 3D irradiance probe volume:
    //   - 7 x Texture3D<RGBA16_SFLOAT> storing packed L2 SH coefficients + validity in tex6.w
    //   - Optional Texture3D<RGBA16_SFLOAT> storing per-probe relocation offset + state
    //   - Optional Texture2D<RG16_SFLOAT> octahedral visibility atlas (meanDist, meanDistSq)
    class IGN_API IrradianceVolume
    {
    public:
        IrradianceVolume() = default;
        ~IrradianceVolume() = default;

        bool UploadFromData(const IrradianceVolumeData &data);
        bool LoadFromFile(const std::filesystem::path &path);

        void FillGIGPUData(GI_GPUData &outGI, const IrradianceVolumeComponent *comp = nullptr) const;
        std::vector<gi::DebugProbeGPU> BuildDebugProbes(float radius = 0.22f) const;

        bool IsValid() const;
        const IrradianceVolumeData &GetData() const { return m_Data; }

        Ref<Texture> GetSHTexture(size_t idx) const { return idx < m_SHTextures.size() ? m_SHTextures[idx] : nullptr; }
        Ref<Texture> GetOffsetTexture() const { return m_OffsetTexture; }
        Ref<Texture> GetVisibilityAtlas() const { return m_VisibilityAtlas; }

    private:
        IrradianceVolumeData m_Data;
        std::array<Ref<Texture>, 7> m_SHTextures;
        Ref<Texture> m_OffsetTexture;
        Ref<Texture> m_VisibilityAtlas;
    };
}

#endif

