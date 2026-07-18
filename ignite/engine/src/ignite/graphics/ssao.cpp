// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ssao.hpp"

#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/icamera.hpp"

namespace ignite
{
    SSAO::SSAO(uint32_t width, uint32_t height)
        : m_Width(std::max(width, 1u)), m_Height(std::max(height, 1u))
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::BindingLayoutDesc aoLayout;
        aoLayout.visibility = nvrhi::ShaderType::Compute;
        aoLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // depth
        aoLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // noise
        aoLayout.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0)); // Target UAV
        aoLayout.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // clamp sampler
        aoLayout.addItem(nvrhi::BindingLayoutItem::Sampler(1)); // repeat sampler
        aoLayout.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // params
        m_AOLayout = device->createBindingLayout(aoLayout);

        // Blur layout now includes the depth texture for bilateral filtering.
        nvrhi::BindingLayoutDesc blurLayout;
        blurLayout.visibility = nvrhi::ShaderType::Compute;
        blurLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // raw AO (or previous pass)
        blurLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // depth texture for bilateral
        blurLayout.addItem(nvrhi::BindingLayoutItem::Texture_UAV(0)); // Target UAV
        blurLayout.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // clamp sampler
        blurLayout.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // blur params
        m_BlurLayout = device->createBindingLayout(blurLayout);

        auto clampSamplerDesc = nvrhi::SamplerDesc();
        clampSamplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::ClampToEdge);
        clampSamplerDesc.setAllFilters(false);
        m_ClampSampler = device->createSampler(clampSamplerDesc);

        auto repeatSamplerDesc = nvrhi::SamplerDesc();
        repeatSamplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Repeat);
        repeatSamplerDesc.setAllFilters(false);
        m_RepeatSampler = device->createSampler(repeatSamplerDesc);

        constexpr uint32_t kSSAOCBVersions = 512;
        m_SSAOParamsBuffer = ConstantBuffer::Create(sizeof(SSAOParams), true, kSSAOCBVersions, "SSAO Params");
        m_BlurParamsBuffer = ConstantBuffer::Create(sizeof(BlurParams), true, kSSAOCBVersions, "SSAO Blur Params");

        m_AOComputeShader = Shader::Create("resources/shaders/ssao.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);
        m_BlurComputeShader = Shader::Create("resources/shaders/ssao_blur.compute.hlsl", UMBRA_SHADER_TYPE_COMPUTE, false);

        BuildKernel();
        BuildNoise();
        CreateTextures(m_Width, m_Height);
    }

    SSAO::~SSAO()
    {
        m_AOComputePipeline = nullptr;
        m_BlurComputePipeline = nullptr;
        m_ClampSampler = nullptr;
        m_RepeatSampler = nullptr;
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

    void SSAO::BuildNoise()
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

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        auto pool = device->createCommandList();
        pool->open();

        TextureCreateInfo info;
        info.width = 4;
        info.height = 4;
        info.format = nvrhi::Format::RGBA32_FLOAT;
        info.dimension = nvrhi::TextureDimension::Texture2D;
        info.initialState = nvrhi::ResourceStates::ShaderResource;
        info.samplerAddressU = nvrhi::SamplerAddressMode::Repeat;
        info.samplerAddressV = nvrhi::SamplerAddressMode::Repeat;
        info.samplerLinearFiltering = false;
        info.keepInitialState = true;
        info.bindless = false;

        m_NoiseTexture = Texture::Create(pixelData, info, pool, "SSAO Noise");
        
        pool->close();
        device->executeCommandList(pool);
    }

    void SSAO::CreateTextures(uint32_t width, uint32_t height)
    {
        IGN_PROFILE_SCOPE_COLOR("SSAO::CreateTextures", 0x404040FF);
        m_Width = std::max(width / 2, 1u); // Half resolution optimization
        m_Height = std::max(height / 2, 1u);

        TextureCreateInfo info;
        info.width = m_Width;
        info.height = m_Height;
        info.format = nvrhi::Format::R8_UNORM;
        info.isUAV = true;
        info.isRenderTarget = false;
        info.initialState = nvrhi::ResourceStates::UnorderedAccess;
        info.keepInitialState = true;
        info.bindless = false;

        m_AOTex = Texture::Create(info, "[SSAO Raw UAV]");
        m_BlurTex = Texture::Create(info, "[SSAO Blur UAV]");

        InvalidatePipelines();
    }

    void SSAO::InvalidatePipelines()
    {
        m_AOComputePipeline = nullptr;
        m_BlurComputePipeline = nullptr;
    }

    void SSAO::EnsurePipelines()
    {
        if (m_AOComputePipeline && m_BlurComputePipeline)
        {
            return;
        }

        IGN_PROFILE_SCOPE_COLOR("SSAO::EnsurePipelines", 0x404040FF);
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        auto buildComputePipeline = [&](Ref<Shader> computeShader, nvrhi::BindingLayoutHandle bindingLayout) -> nvrhi::ComputePipelineHandle
        {
            nvrhi::ComputePipelineDesc desc;
            desc.setComputeShader(computeShader->GetHandle());
            desc.addBindingLayout(bindingLayout);
            return device->createComputePipeline(desc);
        };

        m_AOComputePipeline = buildComputePipeline(m_AOComputeShader, m_AOLayout);
        m_BlurComputePipeline = buildComputePipeline(m_BlurComputeShader, m_BlurLayout);
    }

    void SSAO::Build(nvrhi::ICommandList *cmd, const Ref<Texture> &depthTexture, ICamera *camera, 
        const PostProcessing &settings, const Ref<VertexBuffer> &fullscreenVertexBuffer)
    {
        if (!cmd || !depthTexture) return;

        EnsurePipelines();
        if (!m_AOComputePipeline || !m_BlurComputePipeline) return;

        IGN_PROFILE_SCOPE_COLOR("SSAO::Build", 0x404040FF);
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // ===================================================================
        // Pass 1: Raw SSAO Compute
        // ===================================================================
        SSAOParams params;
        params.projection = camera->GetProjection();
        params.projectionInv = glm::inverse(camera->GetProjection());
        for (int i = 0; i < 32; ++i) params.samples[i] = m_Kernel[i];
        params.params = glm::vec4(settings.aoRadius, settings.aoBias, settings.aoPower, 0.0f);
        params.noiseScale = glm::vec4(static_cast<float>(m_Width) / 4.0f, static_cast<float>(m_Height) / 4.0f, 0.0f, 0.0f);

        m_SSAOParamsBuffer->SetData(cmd, &params, sizeof(params));

        cmd->setTextureState(m_AOTex->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
        cmd->commitBarriers();

        auto aoDesc = nvrhi::BindingSetDesc();
        aoDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, depthTexture->GetHandle()));
        aoDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_NoiseTexture->GetHandle()));
        aoDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, m_AOTex->GetHandle()));
        aoDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_ClampSampler));
        aoDesc.addItem(nvrhi::BindingSetItem::Sampler(1, m_RepeatSampler));
        aoDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_SSAOParamsBuffer->GetHandle()));
        nvrhi::BindingSetHandle aoBindingSet = device->createBindingSet(aoDesc, m_AOLayout);

        {
            nvrhi::ComputeState state;
            state.pipeline = m_AOComputePipeline;
            state.bindings = { aoBindingSet };
            cmd->setComputeState(state);
            cmd->dispatch((m_Width + 7) / 8, (m_Height + 7) / 8, 1);
        }

        // ===================================================================
        // Pass 2: Bilateral Blur — Horizontal (m_AOTex → m_BlurTex)
        // ===================================================================
        {
            BlurParams blurParams;
            blurParams.horizontal = 1.0f;
            blurParams.nearPlane = camera->nearPlane;
            blurParams.farPlane = camera->farPlane;
            blurParams.depthSharpness = 10.0f; // bilateral edge sharpness
            m_BlurParamsBuffer->SetData(cmd, &blurParams, sizeof(blurParams));

            cmd->setTextureState(m_AOTex->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
            cmd->setTextureState(m_BlurTex->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
            cmd->commitBarriers();

            auto blurDesc = nvrhi::BindingSetDesc();
            blurDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_AOTex->GetHandle()));
            blurDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, depthTexture->GetHandle()));
            blurDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, m_BlurTex->GetHandle()));
            blurDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_ClampSampler));
            blurDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_BlurParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle blurBindingSetH = device->createBindingSet(blurDesc, m_BlurLayout);

            nvrhi::ComputeState state;
            state.pipeline = m_BlurComputePipeline;
            state.bindings = { blurBindingSetH };
            cmd->setComputeState(state);
            cmd->dispatch((m_Width + 7) / 8, (m_Height + 7) / 8, 1);
        }

        // ===================================================================
        // Pass 3: Bilateral Blur — Vertical (m_BlurTex → m_AOTex)
        // The final blurred AO result ends up in m_AOTex.
        // ===================================================================
        {
            BlurParams blurParams;
            blurParams.horizontal = 0.0f;
            blurParams.nearPlane = camera->nearPlane;
            blurParams.farPlane = camera->farPlane;
            blurParams.depthSharpness = 10.0f;
            m_BlurParamsBuffer->SetData(cmd, &blurParams, sizeof(blurParams));

            cmd->setTextureState(m_BlurTex->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
            cmd->setTextureState(m_AOTex->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::UnorderedAccess);
            cmd->commitBarriers();

            auto blurDesc = nvrhi::BindingSetDesc();
            blurDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_BlurTex->GetHandle()));
            blurDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, depthTexture->GetHandle()));
            blurDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(0, m_AOTex->GetHandle()));
            blurDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_ClampSampler));
            blurDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_BlurParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle blurBindingSetV = device->createBindingSet(blurDesc, m_BlurLayout);

            nvrhi::ComputeState state;
            state.pipeline = m_BlurComputePipeline;
            state.bindings = { blurBindingSetV };
            cmd->setComputeState(state);
            cmd->dispatch((m_Width + 7) / 8, (m_Height + 7) / 8, 1);
        }

        // Final result is in m_AOTex — transition to SRV for composite pass
        cmd->setTextureState(m_AOTex->GetHandle(), nvrhi::TextureSubresourceSet(), nvrhi::ResourceStates::ShaderResource);
        cmd->commitBarriers();
    }

    void SSAO::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) return;
        if (m_Width == width / 2 && m_Height == height / 2) return;
        CreateTextures(width, height);
    }

    Ref<Texture> SSAO::GetAOTexture() const
    {
        // After the two-pass blur (H→m_BlurTex, V→m_AOTex), the final
        // blurred AO result lives in m_AOTex.
        return m_AOTex;
    }
}