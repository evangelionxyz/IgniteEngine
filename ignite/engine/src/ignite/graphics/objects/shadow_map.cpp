// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/binding_cache.hpp"

#include "shadow_map.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/core/device/device_manager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/common.hpp>

namespace ignite
{
    CascadedShadowMap::CascadedShadowMap(ShadowMapQuality quality)
    {
        m_GPUDataBuffer = ConstantBuffer::Create(sizeof(CSM_GPUData), true, 256, "Cascadded ShadowMap");
        m_ModelGPUDataBuffer = ConstantBuffer::Create(sizeof(CSMModel_GPUData), true, 256, "Cascadded Model ShadowMap");

        // Initialize shadow parameters with reasonable defaults
        m_GPUData.shadowStrength = 1.0f;   // full direct shadow attenuation; ambient/IBL remains lit
        m_GPUData.minBias = 0.0002f;       // bias for surfaces facing light directly
        m_GPUData.maxBias = 0.002f;        // bias for steep/grazing-angle surfaces
        m_GPUData.pcfRadius = 1.5f;        // PCF filter radius in texels for cascade 0
        m_GPUData.softShadows = 1;
        
        Resize(quality);
    }

    CascadedShadowMap::~CascadedShadowMap()
    {
        if (m_DepthSampler)
        {
            if (auto *dm = DeviceManager::GetInstance())
            {
                dm->GetNRI().DestroyDescriptor(m_DepthSampler);
            }
            m_DepthSampler = nullptr;
        }
    }

    void CascadedShadowMap::Resize(ShadowMapQuality quality)
    {
        if (quality == m_Quality && !m_DepthTextures.empty())
            return;

        IGN_PROFILE_FUNCTION();

        m_Resolution = ShadowMapQuality::LOW == quality ? 512 :
                       ShadowMapQuality::MEDIUM == quality ? 1024 :
                       ShadowMapQuality::HIGH == quality ? 2048 :
                       ShadowMapQuality::ULTRA == quality ? 4096 :
                       ShadowMapQuality::ULTIMATE == quality ? 8192 : 1024;
        
        m_Quality = quality;
        m_GPUData.shadowTexelSize = 1.0f / static_cast<float>(m_Resolution);

        // Linear clamp sampler for shadow map PCF — used with SampleLevel in the shader.
        // Linear filtering provides smooth interpolation between depth texels across Poisson taps.
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        if (!m_DepthSampler)
        {
            nri::SamplerDesc samplerDesc = {};
            samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::NEAREST };
            samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
            samplerDesc.mipMax = 1.0f;
            nri.CreateSampler(*device, samplerDesc, m_DepthSampler);
            LOG_ASSERT(m_DepthSampler, "Failed to create depth comparison sampler");
        }

        if (!m_DepthTextures.empty())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }

        CreateCascadeFramebuffers();
        Material::InvalidateAllBindingSets();
        BindingCache::Clear();
    }

    void CascadedShadowMap::BeginCascade(nri::CommandBuffer *cmd, int cascadeIndex, uint32_t frameIndex)
    {
        IGN_PROFILE_FUNCTION();

        if (frameIndex < m_CascadeFramebuffers.size() && cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
        {
            if (auto &cascadeRT = m_CascadeFramebuffers[frameIndex][cascadeIndex])
            {
                cascadeRT->ClearDepthAttachment(cmd, 1.0f, 0);
            }
        }
    }

    void CascadedShadowMap::EnsureShaderResourceLayout(nri::CommandBuffer *cmd, uint32_t frameIndex)
    {
        if (!cmd)
            return;

        Ref<Texture> depthTex = GetDepthTexture(frameIndex);
        if (!depthTex || !depthTex->GetHandle())
            return;

        // If cascaded shadows have never been rendered for this frame slot, the depth array is still
        // in its created, never-written layout. Transitioning that straight to a read-only layout is
        // invalid (the contents would be silently discarded), so materialize a defined depth state by
        // clearing every cascade layer through its render target first. This is a one-time cost per slot.
        if (depthTex->GetCurrentLayout() == nri::Layout::UNDEFINED
            && frameIndex < m_CascadeFramebuffers.size())
        {
            for (int cascadeIndex = 0; cascadeIndex < NUM_CASCADES; ++cascadeIndex)
            {
                if (auto &cascadeRT = m_CascadeFramebuffers[frameIndex][cascadeIndex])
                {
                    cascadeRT->ClearDepthAttachment(cmd, 1.0f, 0);
                }
            }
        }

        if (depthTex->GetCurrentLayout() == nri::Layout::SHADER_RESOURCE)
            return;

        nri::TextureBarrierDesc barrier = {};
        barrier.texture = depthTex->GetHandle();
        barrier.before = { depthTex->GetCurrentAccess(), depthTex->GetCurrentLayout(), depthTex->GetCurrentStages() };
        if (barrier.before.layout == nri::Layout::UNDEFINED)
            barrier.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
        barrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::FRAGMENT_SHADER };
        barrier.mipOffset = 0;
        barrier.mipNum = nri::REMAINING;
        barrier.layerOffset = 0;
        barrier.layerNum = nri::REMAINING;
        barrier.planes = nri::PlaneBits::DEPTH;

        nri::BarrierDesc barrierDesc = {};
        barrierDesc.textures = &barrier;
        barrierDesc.textureNum = 1;
        DeviceManager::GetInstance()->GetCoreInterface().CmdBarrier(*cmd, barrierDesc);

        depthTex->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::FRAGMENT_SHADER);
    }

    Ref<Texture> CascadedShadowMap::GetDepthTexture(uint32_t frameIndex) const
    {
        if (frameIndex < m_DepthTextures.size())
            return m_DepthTextures[frameIndex];
        return nullptr;
    }

    void CascadedShadowMap::ComputeMatrices(ICamera *camera, const glm::vec3 &lightPosition, float shadowDistance, int activeCascades)
    {
        IGN_PROFILE_FUNCTION();

        activeCascades = std::clamp(activeCascades, 1, NUM_CASCADES);

        glm::vec3 lightDir = lightPosition;
        if (glm::dot(lightDir, lightDir) < 1e-6f)
        {
            lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
        }
        else
        {
            lightDir = -glm::normalize(lightDir);
        }

        // Practical split scheme (Engel 2006).
        // λ=0.85 biases heavily toward logarithmic splits so the near cascade
        // gets the highest texel density (crispest shadows close to the camera).
        constexpr float splitLambda = 0.85f;

        // The CSM near plane must be large enough for the log/uniform ratio to
        // produce meaningful splits. With nearPlane=0.1 and farPlane=1000, the
        // ratio is 10000 and the log term becomes negligible — the splits
        // degenerate to nearly uniform, wasting resolution. Clamping to ≥0.5
        // keeps the ratio reasonable (≤400 for shadowDistance=200).
        const float csmNear = glm::max(camera->nearPlane, 0.5f);

        // Use shadowDistance instead of camera->farPlane for CSM coverage.
        // The camera needs a large far plane (1000) to render distant geometry,
        // but shadows only need to extend ~150–300 units from the camera.
        // Using the full far plane spreads the shadow budget over too much
        // depth, making every cascade blurry.
        m_ShadowDistance = glm::max(shadowDistance, csmNear + 1.0f);
        const float csmFar = m_ShadowDistance;

        const float range = csmFar - csmNear;
        const float ratio = csmFar / csmNear;

        std::array<float, NUM_CASCADES> cascadeSplits{};
        for (int i = 0; i < activeCascades; ++i)
        {
            const float p            = (i + 1) / static_cast<float>(activeCascades);
            const float logSplit     = csmNear * std::pow(ratio, p);
            const float uniformSplit = csmNear + range * p;
            const float dist         = splitLambda * (logSplit - uniformSplit) + uniformSplit;
            cascadeSplits[i]           = dist;
            m_GPUData.cascadeSplits[i] = dist;
        }
        for (int i = activeCascades; i < NUM_CASCADES; ++i)
        {
            cascadeSplits[i]           = 1e9f;
            m_GPUData.cascadeSplits[i] = 1e9f;
            m_CascadeTexelSizes[i]     = 0.0f;
        }

        glm::mat4 invView = glm::inverse(camera->GetView());

        const uint32_t vpW = std::max(1u, camera->GetViewportSize().x);
        const uint32_t vpH = std::max(1u, camera->GetViewportSize().y);
        const float aspect = static_cast<float>(vpW) / static_cast<float>(vpH);
        const bool isOrtho = (camera->projectionType == ProjectionType::Orthographic);
        const float fovRadians = glm::radians(camera->fov);
        const glm::mat4 &camProj = camera->GetProjection();
        const bool hasValidProj = (std::abs(camProj[0][0]) > 1e-5f && std::abs(camProj[1][1]) > 1e-5f);
        const float tanHalfFovY = (!isOrtho && hasValidProj) ? (1.0f / std::abs(camProj[1][1])) : std::tan(fovRadians * 0.5f);
        const float tanHalfFovX = (!isOrtho && hasValidProj) ? (1.0f / std::abs(camProj[0][0])) : (tanHalfFovY * aspect);
        const float projOrthoHalfH = (isOrtho && hasValidProj) ? (1.0f / std::abs(camProj[1][1])) : (camera->orthoSize * 0.5f);
        const float projOrthoHalfW = (isOrtho && hasValidProj) ? (1.0f / std::abs(camProj[0][0])) : (projOrthoHalfH * aspect);

        // Fixed world-space up vector so camera pitch/yaw never rotates light space or causes swimming/clipping
        glm::vec3 upDir = (std::abs(lightDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        float lastSplitDist = csmNear;

        for (int cascadeIdx = 0; cascadeIdx < activeCascades; ++cascadeIdx)
        {
            const float splitDist = cascadeSplits[cascadeIdx];
            const float nearDist = lastSplitDist;
            const float farDist = splitDist;

            const float nearX = isOrtho ? projOrthoHalfW : (tanHalfFovX * nearDist);
            const float nearY = isOrtho ? projOrthoHalfH : (tanHalfFovY * nearDist);
            const float farX  = isOrtho ? projOrthoHalfW : (tanHalfFovX * farDist);
            const float farY  = isOrtho ? projOrthoHalfH : (tanHalfFovY * farDist);

            std::array<glm::vec3, 8> frustumCornersVS{
                glm::vec3(-nearX, -nearY, -nearDist),
                glm::vec3(nearX, -nearY, -nearDist),
                glm::vec3(nearX,  nearY, -nearDist),
                glm::vec3(-nearX,  nearY, -nearDist),
                glm::vec3(-farX,  -farY,  -farDist),
                glm::vec3(farX,  -farY,  -farDist),
                glm::vec3(farX,   farY,  -farDist),
                glm::vec3(-farX,   farY,  -farDist)
            };

            std::array<glm::vec3, 8> frustumCornersWS;
            for (size_t iCorner = 0; iCorner < frustumCornersVS.size(); ++iCorner)
            {
                glm::vec4 worldCorner = invView * glm::vec4(frustumCornersVS[iCorner], 1.0f);
                frustumCornersWS[iCorner] = glm::vec3(worldCorner);
            }

            glm::vec3 frustumCenter(0.0f);
            for (const auto &corner : frustumCornersWS)
                frustumCenter += corner;
            frustumCenter /= static_cast<float>(frustumCornersWS.size());

            // Bounding sphere around the frustum slice — rotation-invariant so
            // the ortho extent doesn't flicker when the camera rotates.
            float radius = 0.0f;
            for (const auto &corner : frustumCornersWS)
                radius = glm::max(radius, glm::length(corner - frustumCenter));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            // Use a fixed origin-centered light rotation matrix so texel grid alignment
            // depends only on light direction and never drifts or flips with camera orientation.
            const glm::mat4 lightRot = glm::lookAt(glm::vec3(0.0f), lightDir, upDir);

            const float extent = radius;
            const float texelBase = (extent * 2.0f) / static_cast<float>(m_Resolution);
            const float safeExtent = extent + texelBase;
            const float texelSize = (safeExtent * 2.0f) / static_cast<float>(m_Resolution);
            m_CascadeTexelSizes[cascadeIdx] = texelSize;

            glm::vec3 centerLS = glm::vec3(lightRot * glm::vec4(frustumCenter, 1.0f));
            if (texelSize > 0.0f)
            {
                centerLS.x = std::round(centerLS.x / texelSize) * texelSize;
                centerLS.y = std::round(centerLS.y / texelSize) * texelSize;
            }

            // Rotation-invariant Z bounds from the sub-frustum bounding sphere + upstream/downstream padding
            // so farPlaneLS - nearPlaneLS never fluctuates as the camera rotates.
            const float zPadding = glm::max(150.0f, m_ShadowDistance * 0.5f);
            const float minZ_LS = centerLS.z - safeExtent - zPadding;
            const float maxZ_LS = centerLS.z + safeExtent + zPadding;

            // Shift lightView along Z so every point in [minZ_LS, maxZ_LS] sits strictly in front
            // of nearPlaneLS = 1.0f, eliminating near-plane clamping/clipping behind lightPos.
            const float nearPlaneLS = 1.0f;
            const float farPlaneLS = glm::max(nearPlaneLS + 1.0f, (maxZ_LS - minZ_LS) + nearPlaneLS);
            const float zShift = -(maxZ_LS + nearPlaneLS);

            const glm::mat4 lightView = glm::translate(glm::mat4(1.0f), glm::vec3(-centerLS.x, -centerLS.y, zShift)) * lightRot;
            const glm::mat4 lightProj = glm::orthoZO(-safeExtent, safeExtent, -safeExtent, safeExtent, nearPlaneLS, farPlaneLS);

            m_GPUData.lightViewProj[cascadeIdx] = lightProj * lightView;

            lastSplitDist = splitDist;
        }
        for (int cascadeIdx = activeCascades; cascadeIdx < NUM_CASCADES; ++cascadeIdx)
        {
            m_GPUData.lightViewProj[cascadeIdx] = m_GPUData.lightViewProj[activeCascades - 1];
        }
    }

    void CascadedShadowMap::ComputeBakeMatrix(const AABB &sceneBounds, const glm::vec3 &lightDirection)
    {
        IGN_PROFILE_FUNCTION();

        glm::vec3 lightDir = lightDirection;
        if (glm::dot(lightDir, lightDir) < 1e-6f)
        {
            lightDir = glm::vec3(0.0f, -1.0f, 0.0f);
        }
        else
        {
            lightDir = -glm::normalize(lightDir);
        }

        const glm::vec3 upDir = (std::abs(lightDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::mat4 lightRot = glm::lookAt(glm::vec3(0.0f), lightDir, upDir);

        glm::vec3 bMin = glm::min(sceneBounds.min, sceneBounds.max);
        glm::vec3 bMax = glm::max(sceneBounds.min, sceneBounds.max);
        const glm::vec3 bSize = bMax - bMin;
        if (bSize.x < 1.0f || bSize.y < 1.0f || bSize.z < 1.0f)
        {
            const glm::vec3 center = (bMin + bMax) * 0.5f;
            const glm::vec3 halfExt = glm::max(bSize * 0.5f, glm::vec3(0.5f));
            bMin = center - halfExt;
            bMax = center + halfExt;
        }

        const std::array<glm::vec3, 8> corners{
            glm::vec3(bMin.x, bMin.y, bMin.z),
            glm::vec3(bMax.x, bMin.y, bMin.z),
            glm::vec3(bMin.x, bMax.y, bMin.z),
            glm::vec3(bMax.x, bMax.y, bMin.z),
            glm::vec3(bMin.x, bMin.y, bMax.z),
            glm::vec3(bMax.x, bMin.y, bMax.z),
            glm::vec3(bMin.x, bMax.y, bMax.z),
            glm::vec3(bMax.x, bMax.y, bMax.z),
        };

        glm::vec3 lsMin(std::numeric_limits<float>::max());
        glm::vec3 lsMax(std::numeric_limits<float>::lowest());
        for (const glm::vec3 &corner : corners)
        {
            const glm::vec3 cornerLS = glm::vec3(lightRot * glm::vec4(corner, 1.0f));
            lsMin = glm::min(lsMin, cornerLS);
            lsMax = glm::max(lsMax, cornerLS);
        }

        const float xySpan = glm::max(lsMax.x - lsMin.x, lsMax.y - lsMin.y);
        const float padXY = glm::max(1.0f, xySpan / static_cast<float>(std::max(m_Resolution, 256)) * 2.0f);
        const float zPadding = 16.0f;

        const float minZ_LS = lsMin.z - zPadding;
        const float maxZ_LS = lsMax.z + zPadding;
        const float nearPlaneLS = 1.0f;
        const float farPlaneLS = glm::max(nearPlaneLS + 1.0f, (maxZ_LS - minZ_LS) + nearPlaneLS);
        const float zShift = -(maxZ_LS + nearPlaneLS);

        const glm::mat4 lightView = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zShift)) * lightRot;
        const glm::mat4 lightProj = glm::orthoZO(
            lsMin.x - padXY, lsMax.x + padXY,
            lsMin.y - padXY, lsMax.y + padXY,
            nearPlaneLS, farPlaneLS);

        const glm::mat4 bakeVP = lightProj * lightView;
        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            m_GPUData.lightViewProj[i] = bakeVP;
            m_GPUData.cascadeSplits[i] = 1e9f;
        }
        m_GPUData.shadowTexelSize = (m_Resolution > 0) ? (1.0f / static_cast<float>(m_Resolution)) : (1.0f / 2048.0f);
    }

    RenderTarget *CascadedShadowMap::GetCascadeFramebuffer(int cascadeIndex, uint32_t frameIndex) const
    {
        if (frameIndex < m_CascadeFramebuffers.size() && cascadeIndex >= 0 && cascadeIndex < NUM_CASCADES)
            return m_CascadeFramebuffers[frameIndex][cascadeIndex].get();
        return nullptr;
    }

    void CascadedShadowMap::CreateCascadeFramebuffers()
    {
        IGN_PROFILE_FUNCTION();

        uint32_t maxFrames = DeviceManager::GetInstance()->GetDeviceParameters().maxFramesInFlight;

        m_DepthTextures.resize(maxFrames);
        m_CascadeFramebuffers.resize(maxFrames);

        for (uint32_t f = 0; f < maxFrames; ++f)
        {
            constexpr nri::Format depthFormat = nri::Format::D32_SFLOAT;

            TextureCreateInfo depthCI;
            depthCI.width = m_Resolution;
            depthCI.height = m_Resolution;
            depthCI.isRenderTarget = true;
            depthCI.mipLevels = 1;
            depthCI.arraySize = NUM_CASCADES;
            depthCI.format = depthFormat;
            depthCI.dimension = nri::TextureType::TEXTURE_2D;
            // depthCI.initialState = nri::Layout::DEPTH_STENCIL_ATTACHMENT;
            depthCI.keepInitialState = true;

            m_DepthTextures[f] = Texture::Create(depthCI, std::format("[CSM] Depth Array Frame {}", f));

            // Create a RenderTarget for each cascade layer
            for (int i = 0; i < NUM_CASCADES; ++i)
            {
                RenderTargetCreateInfo rtCI;
                rtCI.width = m_Resolution;
                rtCI.height = m_Resolution;
                rtCI.depthAttachmentOverride = m_DepthTextures[f];

                FramebufferAttachments depthAtt;
                depthAtt.name = "Cascade Depth Attachment";
                depthAtt.format = depthFormat;
                depthAtt.state = nri::Layout::DEPTH_STENCIL_ATTACHMENT;
                depthAtt.mipLevel = 0;
                depthAtt.baseArrayLayer = static_cast<uint32_t>(i);
                depthAtt.arrayLayers = 1;
                rtCI.attachments = { depthAtt };

                m_CascadeFramebuffers[f][i] = RenderTarget::Create(rtCI, std::format("[CSM] Frame {} Cascade {}", f, i));
                m_CascadeFramebuffers[f][i]->SetClearDepthAttachment(1.0f, 0);
            }
        }
    }
}
