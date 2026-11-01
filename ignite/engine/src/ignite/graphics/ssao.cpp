// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ssao.hpp"
#include "binding_cache.hpp"

#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/icamera.hpp"

namespace ignite
{
    namespace
    {
        void TransitionTexture(nri::CommandBuffer *cmd, nri::Texture *texture, nri::Layout beforeLayout, nri::Layout afterLayout)
        {
            if (!cmd || !texture || beforeLayout == afterLayout)
                return;

            nri::AccessBits beforeAccess = nri::AccessBits::NONE;
            nri::StageBits beforeStages = nri::StageBits::ALL;
            if (beforeLayout == nri::Layout::SHADER_RESOURCE)
            {
                beforeAccess = nri::AccessBits::SHADER_RESOURCE;
                beforeStages = nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER;
            }
            else if (beforeLayout == nri::Layout::SHADER_RESOURCE_STORAGE)
            {
                beforeAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
                beforeStages = nri::StageBits::COMPUTE_SHADER;
            }

            nri::AccessBits afterAccess = nri::AccessBits::NONE;
            nri::StageBits afterStages = nri::StageBits::ALL;
            if (afterLayout == nri::Layout::SHADER_RESOURCE)
            {
                afterAccess = nri::AccessBits::SHADER_RESOURCE;
                afterStages = nri::StageBits::COMPUTE_SHADER | nri::StageBits::FRAGMENT_SHADER;
            }
            else if (afterLayout == nri::Layout::SHADER_RESOURCE_STORAGE)
            {
                afterAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
                afterStages = nri::StageBits::COMPUTE_SHADER;
            }

            nri::TextureBarrierDesc barrier = {};
            barrier.texture = texture;
            barrier.before = { beforeAccess, beforeLayout, beforeStages };
            barrier.after = { afterAccess, afterLayout, afterStages };
            barrier.layerNum = 1;
            barrier.mipNum = 1;

            nri::BarrierDesc barrierGroup = {};
            barrierGroup.textures = &barrier;
            barrierGroup.textureNum = 1;
            DeviceManager::GetInstance()->GetCoreInterface().CmdBarrier(*cmd, barrierGroup);
        }
    }

    SSAO::SSAO(nri::CommandBuffer *cmd, uint32_t width, uint32_t height)
        : m_Width(std::max(width, 1u)), m_Height(std::max(height, 1u))
    {
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm->GetDevice();
        const auto &nri = dm->GetCoreInterface();

        m_AOLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::TextureSRV(0))            // depth
            .AddItem(BindingLayoutItem::TextureSRV(1))            // noise
            .AddItem(BindingLayoutItem::Texture_UAV(0))            // Target UAV
            .AddItem(BindingLayoutItem::Sampler(0))                // clamp sampler
            .AddItem(BindingLayoutItem::Sampler(1))                // repeat sampler
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0));// params

        // Blur layout includes the depth texture for bilateral filtering.
        m_BlurLayout.SetVisibility(nri::StageBits::COMPUTE_SHADER)
            .AddItem(BindingLayoutItem::TextureSRV(0))            // raw AO (or previous pass)
            .AddItem(BindingLayoutItem::TextureSRV(1))            // depth texture for bilateral
            .AddItem(BindingLayoutItem::Texture_UAV(0))            // Target UAV
            .AddItem(BindingLayoutItem::Sampler(0))                // clamp sampler
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0));// blur params

        nri::SamplerDesc clampSamplerDesc = {};
        clampSamplerDesc.filters = { nri::Filter::NEAREST, nri::Filter::NEAREST, nri::Filter::NEAREST };
        clampSamplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        nri.CreateSampler(*device, clampSamplerDesc, m_ClampSampler);

        nri::SamplerDesc repeatSamplerDesc = {};
        repeatSamplerDesc.filters = { nri::Filter::NEAREST, nri::Filter::NEAREST, nri::Filter::NEAREST };
        repeatSamplerDesc.addressModes = { nri::AddressMode::REPEAT, nri::AddressMode::REPEAT, nri::AddressMode::REPEAT };
        nri.CreateSampler(*device, repeatSamplerDesc, m_RepeatSampler);

        constexpr uint32_t kSSAOCBVersions = 1;
        m_SSAOParamsBuffer = ConstantBuffer::Create(sizeof(SSAOParams), true, kSSAOCBVersions, "HBAO Params");
        m_BlurHorizontalParamsBuffer = ConstantBuffer::Create(sizeof(BlurParams), true, kSSAOCBVersions, "HBAO Blur H Params");
        m_BlurVerticalParamsBuffer = ConstantBuffer::Create(sizeof(BlurParams), true, kSSAOCBVersions, "HBAO Blur V Params");

        m_AOComputeShader = Shader::Create("resources/shaders/ssao.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_BlurComputeShader = Shader::Create("resources/shaders/ssao_blur.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

        BuildKernel();
        BuildNoise(cmd);
        CreateTextures(m_Width, m_Height);
    }

    SSAO::~SSAO()
    {
        InvalidatePipelines();
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            const auto &nri = dm->GetCoreInterface();
            if (m_ClampSampler)
            {
                nri.DestroyDescriptor(m_ClampSampler);
                m_ClampSampler = nullptr;
            }
            if (m_RepeatSampler)
            {
                nri.DestroyDescriptor(m_RepeatSampler);
                m_RepeatSampler = nullptr;
            }
        }
    }

    void SSAO::BuildKernel()
    {
        m_Kernel.clear();
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::default_random_engine rng;

        for (int i = 0; i < 32; ++i)
        {
            glm::vec3 sample(
                dist(rng) * 2.0f - 1.0f,
                dist(rng) * 2.0f - 1.0f,
                dist(rng) // Hemisphere (z >= 0)
            );
            sample = glm::normalize(sample);
            sample *= dist(rng);
            float scale = float(i) / 32.0f;
            scale = glm::mix(0.1f, 1.0f, scale * scale);
            sample *= scale;
            m_Kernel.push_back(glm::vec4(sample, 0.0f));
        }
    }

    void SSAO::BuildNoise(nri::CommandBuffer *cmd)
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::default_random_engine rng;

        std::vector<glm::vec4> noise;
        noise.reserve(16);
        for (int i = 0; i < 16; ++i)
        {
            noise.emplace_back(
                dist(rng) * 2.0f - 1.0f,
                dist(rng) * 2.0f - 1.0f,
                0.0f,
                1.0f
            );
        }

        std::vector<uint8_t> pixelData;
        pixelData.resize(16 * sizeof(glm::vec4));
        std::memcpy(pixelData.data(), noise.data(), pixelData.size());

        TextureCreateInfo info;
        info.width = 4;
        info.height = 4;
        info.format = nri::Format::RGBA32_SFLOAT;
        info.dimension = nri::TextureType::TEXTURE_2D;
        info.initialLayout = nri::Layout::SHADER_RESOURCE;
        info.samplerAddressU = nri::AddressMode::REPEAT;
        info.samplerAddressV = nri::AddressMode::REPEAT;
        info.samplerLinearFiltering = false;
        info.bindless = false;

        m_NoiseTexture = Texture::Create(pixelData, info, cmd, "HBAO Noise");
    }

    void SSAO::CreateTextures(uint32_t width, uint32_t height)
    {
        IGN_PROFILE_SCOPE_COLOR("HBAO::CreateTextures", 0x404040FF);
        m_Width = std::max(width / 2, 1u); // Half resolution optimization
        m_Height = std::max(height / 2, 1u);

        TextureCreateInfo info;
        info.width = m_Width;
        info.height = m_Height;
        info.format = nri::Format::R8_UNORM;
        info.isUAV = true;
        info.isRenderTarget = false;
        info.initialLayout = nri::Layout::SHADER_RESOURCE;
        info.bindless = false;

        m_AOTex = Texture::Create(info, "[SSAO Raw UAV]");
        m_BlurTex = Texture::Create(info, "[SSAO Blur UAV]");

        InvalidatePipelines();
    }

    void SSAO::InvalidatePipelines()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            const auto &nri = dm->GetCoreInterface();
            if (m_AOComputePipeline)
            {
                nri.DestroyPipeline(m_AOComputePipeline);
                m_AOComputePipeline = nullptr;
            }
            if (m_BlurComputePipeline)
            {
                nri.DestroyPipeline(m_BlurComputePipeline);
                m_BlurComputePipeline = nullptr;
            }
            if (m_AOPipelineLayout)
            {
                nri.DestroyPipelineLayout(m_AOPipelineLayout);
                m_AOPipelineLayout = nullptr;
            }
            if (m_BlurPipelineLayout)
            {
                nri.DestroyPipelineLayout(m_BlurPipelineLayout);
                m_BlurPipelineLayout = nullptr;
            }
        }
    }

    void SSAO::EnsurePipelines()
    {
        if (m_AOComputePipeline && m_BlurComputePipeline)
        {
            return;
        }

        IGN_PROFILE_SCOPE_COLOR("HBAO::EnsurePipelines", 0x404040FF);
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm->GetDevice();
        const auto &nri = dm->GetCoreInterface();

        m_AOPipelineLayout = m_AOLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        nri::ComputePipelineDesc aoDesc = {};
        aoDesc.pipelineLayout = m_AOPipelineLayout;
        aoDesc.shader = m_AOComputeShader->GetShaderDesc();
        nri.CreateComputePipeline(*device, aoDesc, m_AOComputePipeline);

        m_BlurPipelineLayout = m_BlurLayout.CreatePipelineLayout(nri::StageBits::COMPUTE_SHADER, device);
        nri::ComputePipelineDesc blurDesc = {};
        blurDesc.pipelineLayout = m_BlurPipelineLayout;
        blurDesc.shader = m_BlurComputeShader->GetShaderDesc();
        nri.CreateComputePipeline(*device, blurDesc, m_BlurComputePipeline);
    }

    void SSAO::Build(nri::CommandBuffer *cmd, const Ref<Texture> &depthTexture, ICamera *camera,
        const PostProcessing &settings, const Ref<VertexBuffer> &fullscreenVertexBuffer)
    {
        (void)fullscreenVertexBuffer;
        if (!cmd || !depthTexture)
            return;

        EnsurePipelines();
        if (!m_AOComputePipeline || !m_BlurComputePipeline)
            return;

        IGN_PROFILE_SCOPE_COLOR("HBAO::Build", 0x404040FF);
        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        // ===================================================================
        // Pass 1: Raw SSAO Compute
        // ===================================================================
        SSAOParams params;
        params.projection = camera->GetProjection();
        params.projectionInv = glm::inverse(camera->GetProjection());
        for (int i = 0; i < 32; ++i)
            params.samples[i] = m_Kernel[i];
        params.params = glm::vec4(settings.aoRadius, settings.aoBias, settings.aoPower, 0.0f);
        params.noiseScale = glm::vec4(static_cast<float>(m_Width) / 4.0f, static_cast<float>(m_Height) / 4.0f, 0.0f, 0.0f);

        m_SSAOParamsBuffer->SetData(cmd, &params, sizeof(params));

        TransitionTexture(cmd, m_AOTex->GetHandle(), nri::Layout::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE_STORAGE);

        BindingSetDesc aoDesc;
        aoDesc.AddItem(BindingSetItem::TextureSRV(0, depthTexture->GetShaderResourceView()));
        aoDesc.AddItem(BindingSetItem::TextureSRV(1, m_NoiseTexture->GetShaderResourceView()));
        aoDesc.AddItem(BindingSetItem::TextureUAV(0, m_AOTex->GetStorageView(0)));
        aoDesc.AddItem(BindingSetItem::Sampler(0, m_ClampSampler));
        aoDesc.AddItem(BindingSetItem::Sampler(1, m_RepeatSampler));
        aoDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_SSAOParamsBuffer->GetDescriptor()));
        Ref<BindingSet> aoBindingSet = BindingCache::GetOrCreateBindingSet(aoDesc, m_AOPipelineLayout, 0, "SSAO Compute");

        {
            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_AOPipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_AOComputePipeline);

            nri::SetDescriptorSetDesc setDesc = {};
            setDesc.setIndex = 0;
            setDesc.descriptorSet = aoBindingSet->GetHandle();
            setDesc.bindPoint = nri::BindPoint::COMPUTE;
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc dispatchDesc = { (m_Width + 7) / 8, (m_Height + 7) / 8, 1 };
            nri.CmdDispatch(*cmd, dispatchDesc);
        }

        // ===================================================================
        // Pass 2: Bilateral Blur — Horizontal (m_AOTex → m_BlurTex)
        // ===================================================================
        {
            BlurParams blurParams;
            blurParams.horizontal = 1.0f;
            blurParams.nearPlane = camera->nearPlane;
            blurParams.farPlane = camera->farPlane;
            blurParams.depthSharpness = 10.0f;
            m_BlurHorizontalParamsBuffer->SetData(cmd, &blurParams, sizeof(blurParams));

            TransitionTexture(cmd, m_AOTex->GetHandle(), nri::Layout::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE);
            TransitionTexture(cmd, m_BlurTex->GetHandle(), nri::Layout::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE_STORAGE);

            BindingSetDesc blurDesc;
            blurDesc.AddItem(BindingSetItem::TextureSRV(0, m_AOTex->GetShaderResourceView()));
            blurDesc.AddItem(BindingSetItem::TextureSRV(1, depthTexture->GetShaderResourceView()));
            blurDesc.AddItem(BindingSetItem::TextureUAV(0, m_BlurTex->GetStorageView(0)));
            blurDesc.AddItem(BindingSetItem::Sampler(0, m_ClampSampler));
            blurDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_BlurHorizontalParamsBuffer->GetDescriptor()));
            Ref<BindingSet> blurBindingSetH = BindingCache::GetOrCreateBindingSet(blurDesc, m_BlurPipelineLayout, 0, "SSAO Blur H");

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_BlurPipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_BlurComputePipeline);

            nri::SetDescriptorSetDesc setDesc = {};
            setDesc.setIndex = 0;
            setDesc.descriptorSet = blurBindingSetH->GetHandle();
            setDesc.bindPoint = nri::BindPoint::COMPUTE;
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc dispatchDesc = { (m_Width + 7) / 8, (m_Height + 7) / 8, 1 };
            nri.CmdDispatch(*cmd, dispatchDesc);
        }

        // ===================================================================
        // Pass 3: Bilateral Blur — Vertical (m_BlurTex → m_AOTex)
        // ===================================================================
        {
            BlurParams blurParams;
            blurParams.horizontal = 0.0f;
            blurParams.nearPlane = camera->nearPlane;
            blurParams.farPlane = camera->farPlane;
            blurParams.depthSharpness = 10.0f;
            m_BlurVerticalParamsBuffer->SetData(cmd, &blurParams, sizeof(blurParams));

            TransitionTexture(cmd, m_BlurTex->GetHandle(), nri::Layout::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE);
            TransitionTexture(cmd, m_AOTex->GetHandle(), nri::Layout::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE_STORAGE);

            BindingSetDesc blurDesc;
            blurDesc.AddItem(BindingSetItem::TextureSRV(0, m_BlurTex->GetShaderResourceView()));
            blurDesc.AddItem(BindingSetItem::TextureSRV(1, depthTexture->GetShaderResourceView()));
            blurDesc.AddItem(BindingSetItem::TextureUAV(0, m_AOTex->GetStorageView(0)));
            blurDesc.AddItem(BindingSetItem::Sampler(0, m_ClampSampler));
            blurDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_BlurVerticalParamsBuffer->GetDescriptor()));
            Ref<BindingSet> blurBindingSetV = BindingCache::GetOrCreateBindingSet(blurDesc, m_BlurPipelineLayout, 0, "SSAO Blur V");

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::COMPUTE, *m_BlurPipelineLayout);
            nri.CmdSetPipeline(*cmd, *m_BlurComputePipeline);

            nri::SetDescriptorSetDesc setDesc = {};
            setDesc.setIndex = 0;
            setDesc.descriptorSet = blurBindingSetV->GetHandle();
            setDesc.bindPoint = nri::BindPoint::COMPUTE;
            nri.CmdSetDescriptorSet(*cmd, setDesc);

            nri::DispatchDesc dispatchDesc = { (m_Width + 7) / 8, (m_Height + 7) / 8, 1 };
            nri.CmdDispatch(*cmd, dispatchDesc);
        }

        // Final result is in m_AOTex — transition to SRV for composite pass
        TransitionTexture(cmd, m_AOTex->GetHandle(), nri::Layout::SHADER_RESOURCE_STORAGE, nri::Layout::SHADER_RESOURCE);
    }

    void SSAO::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        if (m_Width == std::max(width / 2, 1u) && m_Height == std::max(height / 2, 1u))
            return;
        CreateTextures(width, height);
    }

    Ref<Texture> SSAO::GetAOTexture() const
    {
        return m_AOTex;
    }
}
