// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"

#include "shadow_map.hpp"
#include "ignite/scene/icamera.hpp"
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

        CreateCascadeFramebuffers();
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

    void CascadedShadowMap::ComputeMatrices(ICamera *camera, const glm::vec3 &lightPosition, float shadowDistance)
    {
        IGN_PROFILE_FUNCTION();

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
        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            const float p            = (i + 1) / static_cast<float>(NUM_CASCADES);
            const float logSplit     = csmNear * std::pow(ratio, p);
            const float uniformSplit = csmNear + range * p;
            const float dist         = splitLambda * (logSplit - uniformSplit) + uniformSplit;
            cascadeSplits[i]           = dist;
            m_GPUData.cascadeSplits[i] = dist;
        }

        glm::mat4 invView = glm::inverse(camera->GetView());

        const uint32_t vpW = std::max(1u, camera->GetViewportSize().x);
        const uint32_t vpH = std::max(1u, camera->GetViewportSize().y);
        const float aspect = static_cast<float>(vpW) / static_cast<float>(vpH);
        const bool isOrtho = (camera->projectionType == ProjectionType::Orthographic);
        const float fovRadians = glm::radians(camera->fov);
        const float tanHalfFovY = std::tan(fovRadians * 0.5f);
        const float tanHalfFovX = tanHalfFovY * aspect;

        // Fixed world-space up vector so camera pitch/yaw never rotates light space or causes swimming/clipping
        glm::vec3 upDir = (std::abs(lightDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        float lastSplitDist = csmNear;

        for (int cascadeIdx = 0; cascadeIdx < NUM_CASCADES; ++cascadeIdx)
        {
            const float splitDist = cascadeSplits[cascadeIdx];
            const float nearDist = lastSplitDist;
            const float farDist = splitDist;

            const float orthoHalfH = camera->orthoSize * 0.5f;
            const float orthoHalfW = orthoHalfH * aspect;
            const float nearX = isOrtho ? orthoHalfW : (tanHalfFovX * nearDist);
            const float nearY = isOrtho ? orthoHalfH : (tanHalfFovY * nearDist);
            const float farX  = isOrtho ? orthoHalfW : (tanHalfFovX * farDist);
            const float farY  = isOrtho ? orthoHalfH : (tanHalfFovY * farDist);

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

            auto computeCascadeBounds = [&](const glm::mat4 &lightViewMatrix, glm::vec3 &outMin, glm::vec3 &outMax)
                {
                    outMin = glm::vec3(std::numeric_limits<float>::max());
                    outMax = glm::vec3(std::numeric_limits<float>::lowest());
                    for (const auto &corner : frustumCornersWS)
                    {
                        glm::vec4 cornerLS = lightViewMatrix * glm::vec4(corner, 1.0f);
                        outMin = glm::min(outMin, glm::vec3(cornerLS));
                        outMax = glm::max(outMax, glm::vec3(cornerLS));
                    }
                };

            // Use a fixed origin-centered light rotation matrix so texel grid alignment
            // depends only on light direction and never drifts or flips with camera orientation.
            const glm::mat4 lightRot = glm::lookAt(glm::vec3(0.0f), lightDir, upDir);

            const float extent = radius;
            const float texelBase = (extent * 2.0f) / static_cast<float>(m_Resolution);
            const float safeExtent = extent + texelBase;
            const float texelSize = (safeExtent * 2.0f) / static_cast<float>(m_Resolution);

            glm::vec3 centerLS = glm::vec3(lightRot * glm::vec4(frustumCenter, 1.0f));
            if (texelSize > 0.0f)
            {
                centerLS.x = std::round(centerLS.x / texelSize) * texelSize;
                centerLS.y = std::round(centerLS.y / texelSize) * texelSize;
            }

            // Measure cascade Z bounds in lightRot space across the 8 frustum corners
            glm::vec3 cascadeMin, cascadeMax;
            computeCascadeBounds(lightRot, cascadeMin, cascadeMax);

            // Pad Z upstream (+Z toward sun) and downstream (-Z away from sun) so tall or
            // distant shadow casters outside the view sub-frustum are never clipped.
            const float zPadding = glm::max(150.0f, m_ShadowDistance * 0.5f);
            const float minZ_LS = cascadeMin.z - zPadding;
            const float maxZ_LS = cascadeMax.z + zPadding;

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
