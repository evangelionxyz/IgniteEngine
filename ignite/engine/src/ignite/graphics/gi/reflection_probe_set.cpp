// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "reflection_probe_set.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/component.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ignite
{
    namespace gi
    {
        glm::vec3 BoxProjectOBB(const glm::vec3 &worldPos, const glm::vec3 &reflectDir,
            const ReflectionProbe_GPUData &probe)
        {
            const glm::vec3 R = (glm::dot(reflectDir, reflectDir) > 1e-8f)
                ? glm::normalize(reflectDir)
                : glm::vec3(0.0f, 1.0f, 0.0f);

            const glm::vec3 localP = glm::vec3(probe.worldToLocal * glm::vec4(worldPos, 1.0f));
            const glm::vec3 localR = glm::mat3(probe.worldToLocal) * R;

            const glm::vec3 safeR(
                std::abs(localR.x) > 1e-6f ? localR.x : (localR.x >= 0.0f ? 1e-6f : -1e-6f),
                std::abs(localR.y) > 1e-6f ? localR.y : (localR.y >= 0.0f ? 1e-6f : -1e-6f),
                std::abs(localR.z) > 1e-6f ? localR.z : (localR.z >= 0.0f ? 1e-6f : -1e-6f));

            const glm::vec3 boxMin = glm::vec3(probe.boxMinAndSlice);
            const glm::vec3 boxMax = glm::vec3(probe.boxMaxAndBlend);

            const glm::vec3 firstPlane = (boxMax - localP) / safeR;
            const glm::vec3 secondPlane = (boxMin - localP) / safeR;
            const glm::vec3 furthestPlane = glm::max(firstPlane, secondPlane);
            const float t = std::min({ furthestPlane.x, furthestPlane.y, furthestPlane.z });
            if (t <= 1e-5f)
            {
                return R;
            }

            const glm::vec3 localHit = localP + localR * t;
            const glm::vec3 worldHit = glm::vec3(probe.localToWorld * glm::vec4(localHit, 1.0f));
            const glm::vec3 dir = worldHit - glm::vec3(probe.capturePosAndNorm);
            const float lenSq = glm::dot(dir, dir);
            return (lenSq > 1e-8f) ? (dir * (1.0f / std::sqrt(lenSq))) : R;
        }

        float ComputeProbeInfluenceWeight(const glm::vec3 &worldPos,
            const ReflectionProbe_GPUData &probe)
        {
            const glm::vec3 localP = glm::vec3(probe.worldToLocal * glm::vec4(worldPos, 1.0f));
            const glm::vec3 infExt = glm::vec3(probe.influenceMaxAndInt);
            const glm::vec3 dist = infExt - glm::abs(localP);
            const float minD = std::min({ dist.x, dist.y, dist.z });
            if (minD <= 0.0f)
            {
                return 0.0f;
            }

            const float blend = probe.boxMaxAndBlend.w;
            if (blend <= 1e-5f)
            {
                return 1.0f;
            }
            const float t = std::clamp(minD / blend, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float ComputeSpecularOcclusion(float ndotV, float ao, float roughness)
        {
            const float a = std::max(0.0f, std::clamp(ndotV, 0.0f, 1.0f) + std::clamp(ao, 0.0f, 1.0f));
            const float exponent = std::exp2(-16.0f * std::clamp(roughness, 0.0f, 1.0f) - 1.0f);
            return std::clamp(std::pow(a, exponent) - 1.0f + std::clamp(ao, 0.0f, 1.0f), 0.0f, 1.0f);
        }

        float ComputeProbeNormalizationFactor(const glm::vec3 &pixelDiffuseGI,
            const glm::vec3 &probeCaptureGI, float strength)
        {
            const glm::vec3 lumWeights(0.2126f, 0.7152f, 0.0722f);
            const float lumPixel = glm::dot(glm::max(pixelDiffuseGI, glm::vec3(0.0f)), lumWeights);
            const float lumProbe = glm::dot(glm::max(probeCaptureGI, glm::vec3(0.0f)), lumWeights);
            const float ratio = (lumProbe > 1e-4f) ? std::clamp(lumPixel / lumProbe, 0.0f, 2.0f) : 1.0f;
            return glm::mix(1.0f, ratio, std::clamp(strength, 0.0f, 1.0f));
        }
    }

    void ReflectionProbeSet::Clear()
    {
        m_CubeArray.reset();
        m_Probes.clear();
        m_Resolution = 128;
        m_MipCount = 1;
    }

    bool ReflectionProbeSet::IsValid() const
    {
        return m_CubeArray != nullptr
            && m_CubeArray->GetBindlessIndex() != UINT32_MAX
            && !m_Probes.empty();
    }

    bool ReflectionProbeSet::BuildFromScene(Scene *scene, const std::filesystem::path &projectRoot)
    {
        Clear();
        if (!scene || !scene->registry)
        {
            return false;
        }

        struct LoadedEntry
        {
            entt::entity entity = entt::null;
            TransformComponent transform;
            ReflectionProbeComponent component;
            ReflectionProbeData data;
            uint64_t uuid = 0;
        };

        std::vector<LoadedEntry> entries;
        auto view = scene->registry->view<TransformComponent, ReflectionProbeComponent>();
        for (entt::entity e : view)
        {
            const auto &tr = view.get<TransformComponent>(e);
            const auto &rp = view.get<ReflectionProbeComponent>(e);
            if (rp.bakedAssetPath.empty())
            {
                continue;
            }

            std::filesystem::path resolved(rp.bakedAssetPath);
            if (resolved.is_relative() && !projectRoot.empty())
            {
                resolved = projectRoot / resolved;
            }

            std::error_code ec;
            if (!std::filesystem::exists(resolved, ec))
            {
                continue;
            }

            ReflectionProbeData data;
            if (!gi_io::LoadReflectionProbe(resolved, data))
            {
                continue;
            }
            if (data.mipFaceBytes.size() != static_cast<size_t>(6u) * data.mipCount)
            {
                continue;
            }

            uint64_t uuidVal = 0;
            if (const auto *id = scene->registry->try_get<IDComponent>(e))
            {
                uuidVal = static_cast<uint64_t>(id->uuid);
            }
            entries.push_back({ e, tr, rp, std::move(data), uuidVal });
        }

        if (entries.empty())
        {
            return false;
        }

        std::sort(entries.begin(), entries.end(), [](const LoadedEntry &a, const LoadedEntry &b)
        {
            if (a.component.priority != b.component.priority)
            {
                return a.component.priority > b.component.priority;
            }
            return a.uuid < b.uuid;
        });

        const uint32_t targetRes = entries[0].data.resolution;
        const uint32_t targetMips = entries[0].data.mipCount;

        std::vector<LoadedEntry> compatible;
        compatible.reserve(std::min<size_t>(entries.size(), MAX_REFLECTION_PROBES));
        for (auto &entry : entries)
        {
            if (compatible.size() >= MAX_REFLECTION_PROBES)
            {
                break;
            }
            if (entry.data.resolution != targetRes || entry.data.mipCount != targetMips)
            {
                LOG_WARN("[ReflectionProbeSet] Skipping probe {} due to resolution/mip mismatch ({}x{} mips vs {}x{} mips)",
                    entry.uuid, entry.data.resolution, entry.data.mipCount, targetRes, targetMips);
                continue;
            }
            compatible.push_back(std::move(entry));
        }

        if (compatible.empty())
        {
            return false;
        }

        TextureCreateInfo arrayInfo;
        arrayInfo.width = targetRes;
        arrayInfo.height = targetRes;
        arrayInfo.depth = 1;
        arrayInfo.mipLevels = targetMips;
        arrayInfo.arraySize = static_cast<uint32_t>(compatible.size()) * 6u;
        arrayInfo.sampleCount = 1;
        arrayInfo.dimension = nri::TextureType::TEXTURE_2D;
        arrayInfo.format = nri::Format::RGBA16_SFLOAT;
        arrayInfo.isCubeMap = true;
        arrayInfo.isRenderTarget = false;
        arrayInfo.isUAV = false;
        arrayInfo.bindless = true;
        arrayInfo.forceBindless = true;
        arrayInfo.samplerLinearFiltering = true;
        arrayInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
        arrayInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
        arrayInfo.samplerAddressW = nri::AddressMode::CLAMP_TO_EDGE;

        m_CubeArray = Texture::Create(arrayInfo, "[GI ReflectionProbe CubeArray]");
        if (!m_CubeArray)
        {
            Clear();
            return false;
        }

        m_Resolution = targetRes;
        m_MipCount = targetMips;
        m_Probes.reserve(compatible.size());

        for (uint32_t sliceIdx = 0; sliceIdx < static_cast<uint32_t>(compatible.size()); ++sliceIdx)
        {
            const LoadedEntry &entry = compatible[sliceIdx];
            if (!m_CubeArray->UploadCubeMipChain(entry.data.mipFaceBytes, sliceIdx))
            {
                LOG_ERROR("[ReflectionProbeSet] Failed uploading mip chain for probe slice {}", sliceIdx);
                Clear();
                return false;
            }

            const glm::mat4 rigidLocalToWorld =
                glm::translate(glm::mat4(1.0f), entry.transform.world.translation) *
                glm::mat4_cast(entry.transform.world.rotation);
            const glm::mat4 rigidWorldToLocal = glm::inverse(rigidLocalToWorld);

            const glm::vec3 worldScale = glm::abs(entry.transform.world.scale);
            const glm::vec3 infExt = glm::max(glm::abs(entry.component.influenceExtents) * worldScale, glm::vec3(0.05f));
            const glm::vec3 projExt = glm::max(glm::abs(entry.component.projectionExtents) * worldScale, glm::vec3(0.05f));
            const glm::vec3 capturePos = entry.transform.world.translation
                + glm::mat3_cast(entry.transform.world.rotation) * (entry.component.captureOffset * worldScale);

            ReflectionProbe_GPUData gpu{};
            gpu.worldToLocal = rigidWorldToLocal;
            gpu.localToWorld = rigidLocalToWorld;
            gpu.boxMinAndSlice = glm::vec4(-projExt, static_cast<float>(sliceIdx));
            gpu.boxMaxAndBlend = glm::vec4(projExt, std::max(entry.component.blendDistance, 0.0f));
            gpu.influenceMinAndPrio = glm::vec4(-infExt, static_cast<float>(entry.component.priority));
            gpu.influenceMaxAndInt = glm::vec4(infExt, std::max(entry.component.intensity, 0.0f));
            gpu.capturePosAndNorm = glm::vec4(capturePos,
                entry.component.normalize ? glm::clamp(entry.component.normalizationStrength, 0.0f, 1.0f) : 0.0f);

            for (int i = 0; i < 4; ++i)
            {
                gpu.captureSH[i] = entry.data.captureSH[i];
            }
            gpu.captureSH[0].w = entry.component.boxProjection ? 1.0f : 0.0f;

            m_Probes.push_back(gpu);
        }

        BindlessSystem::FlushPendingWrites();
        return IsValid();
    }

    void ReflectionProbeSet::FillGIGPUData(GI_GPUData &outGI) const
    {
        if (!IsValid())
        {
            outGI.reflectionCubeArrayIndex = -1;
            outGI.reflectionProbeCount = 0;
            outGI.reflectionMipCount = 1;
            return;
        }

        outGI.reflectionCubeArrayIndex = static_cast<int32_t>(m_CubeArray->GetBindlessIndex());
        outGI.reflectionProbeCount = static_cast<int32_t>(m_Probes.size());
        outGI.reflectionMipCount = static_cast<int32_t>(m_MipCount);
    }
}

