// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_REFLECTION_PROBE_SET_HPP
#define IGN_REFLECTION_PROBE_SET_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/gi/gi_asset_io.hpp"

#include <glm/glm.hpp>
#include <filesystem>
#include <vector>

namespace ignite
{
    class Scene;
    class ReflectionProbeComponent;

    namespace gi
    {
        // CPU-mirror analytic helpers for unit verification and shared OBB parallax / SO formulas
        IGN_API glm::vec3 BoxProjectOBB(const glm::vec3 &worldPos, const glm::vec3 &reflectDir,
            const ReflectionProbe_GPUData &probe);

        IGN_API float ComputeProbeInfluenceWeight(const glm::vec3 &worldPos,
            const ReflectionProbe_GPUData &probe);

        IGN_API float ComputeSpecularOcclusion(float ndotV, float ao, float roughness);

        IGN_API float ComputeProbeNormalizationFactor(const glm::vec3 &pixelDiffuseGI,
            const glm::vec3 &probeCaptureGI, float strength);
    }

    // Manages the active scene's baked reflection probe cubemap array (TextureCubeArray bindless handle)
    // and structured probe descriptor table (ReflectionProbe_GPUData[<= MAX_REFLECTION_PROBES]).
    class IGN_API ReflectionProbeSet
    {
    public:
        ReflectionProbeSet() = default;
        ~ReflectionProbeSet() = default;

        bool BuildFromScene(Scene *scene, const std::filesystem::path &projectRoot = {});
        void Clear();

        void FillGIGPUData(GI_GPUData &outGI) const;

        bool IsValid() const;
        uint32_t GetProbeCount() const { return static_cast<uint32_t>(m_Probes.size()); }
        uint32_t GetResolution() const { return m_Resolution; }
        uint32_t GetMipCount() const { return m_MipCount; }
        const std::vector<ReflectionProbe_GPUData> &GetProbesGPU() const { return m_Probes; }
        Ref<Texture> GetCubeArray() const { return m_CubeArray; }

    private:
        Ref<Texture> m_CubeArray;
        std::vector<ReflectionProbe_GPUData> m_Probes;
        uint32_t m_Resolution = 128;
        uint32_t m_MipCount = 1;
    };
}

#endif

