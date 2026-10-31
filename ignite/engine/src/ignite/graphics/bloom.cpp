// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "bloom.hpp"
#include "binding_cache.hpp"
#include "vertex_data.hpp"

#include "ignite/core/profiler/profiler.hpp"
#include "buffers/vertex_buffer.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "renderer.hpp"

#include "ignite/core/device/device_manager.hpp"

namespace ignite
{
    Bloom::Bloom(int width, int height)
        : m_Width(std::max(width, 1)), m_Height(std::max(height, 1))
    {
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm->GetDevice();
        const auto &nri = dm->GetCoreInterface();

        m_DownsampleLayout.SetVisibility(nri::StageBits::ALL)
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::Sampler(0))
            .AddItem(BindingLayoutItem::ConstantBuffer(0));

        m_BlurLayout.SetVisibility(nri::StageBits::ALL)
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::Sampler(0))
            .AddItem(BindingLayoutItem::ConstantBuffer(0));

        m_UpsampleLayout.SetVisibility(nri::StageBits::ALL)
            .AddItem(BindingLayoutItem::TextureSRV(0))
            .AddItem(BindingLayoutItem::TextureSRV(1))
            .AddItem(BindingLayoutItem::Sampler(0))
            .AddItem(BindingLayoutItem::ConstantBuffer(0));

        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
        samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        samplerDesc.mipMax = 16.0f;
        nri.CreateSampler(*device, samplerDesc, m_Sampler);

        constexpr uint32_t kBloomCBVersions = 1;
        m_BlurHorizontalParamsBuffer = ConstantBuffer::Create(sizeof(BlurParams), false, kBloomCBVersions, "Bloom Blur Horizontal Params");
        m_BlurVerticalParamsBuffer = ConstantBuffer::Create(sizeof(BlurParams), false, kBloomCBVersions, "Bloom Blur Vertical Params");
        m_SingleLevelUpsampleParamsBuffer = ConstantBuffer::Create(sizeof(UpsampleParams), false, kBloomCBVersions, "Bloom Upsample Single Params");

        m_FullscreenVertexShader = Shader::Create("resources/shaders/bloom_fullscreen.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
        m_DownsamplePixelShader = Shader::Create("resources/shaders/bloom_downsample.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);
        m_BlurPixelShader = Shader::Create("resources/shaders/bloom_blur.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);
        m_UpsamplePixelShader = Shader::Create("resources/shaders/bloom_upsample.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

        CreateRenderTargets(m_Width, m_Height);
    }

    Bloom::~Bloom()
    {
        InvalidatePipelines();
        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice() && m_Sampler)
        {
            dm->GetCoreInterface().DestroyDescriptor(m_Sampler);
            m_Sampler = nullptr;
        }
    }

    void Bloom::CreateRenderTargets(uint32_t width, uint32_t height)
    {
        IGN_PROFILE_SCOPE_COLOR("Bloom::CreateRenderTargets", 0xFA0010FF);

        m_Width = std::max(width, 1u);
        m_Height = std::max(height, 1u);

        m_Levels.clear();

        uint32_t w = std::max(m_Width / 2, 1u);
        uint32_t h = std::max(m_Height / 2, 1u);

        RenderTargetCreateInfo levelRTInfo;
        levelRTInfo.attachments =
        {
            FramebufferAttachments{ "[Bloom Color]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT }
        };

        for (int i = 0; i < 8; ++i)
        {
            if (w <= 2 || h <= 2)
            {
                break;
            }

            Level level;
            level.width = static_cast<int>(w);
            level.height = static_cast<int>(h);

            levelRTInfo.width = w;
            levelRTInfo.height = h;
            level.downsampledRT = RenderTarget::Create(levelRTInfo, "[Bloom Downsample RT]");
            level.upsampledRT = RenderTarget::Create(levelRTInfo, "[Bloom Upsample RT]");
            level.blurHorizontalRT = RenderTarget::Create(levelRTInfo, "[Bloom Blur Horizontal RT]");
            level.blurVerticalRT = RenderTarget::Create(levelRTInfo, "[Bloom Blur Vertical RT]");
            level.downsampleParamsBuffer = ConstantBuffer::Create(sizeof(DownsampleParams), false, 1, "Bloom Level Downsample Params");
            level.upsampleParamsBuffer = ConstantBuffer::Create(sizeof(UpsampleParams), false, 1, "Bloom Level Upsample Params");

            m_Levels.emplace_back(level);

            w = std::max(w / 2, 1u);
            h = std::max(h / 2, 1u);
        }

        if (!m_Levels.empty())
        {
            RenderTargetCreateInfo finalRTInfo;
            finalRTInfo.width = static_cast<uint32_t>(m_Levels.front().width);
            finalRTInfo.height = static_cast<uint32_t>(m_Levels.front().height);
            finalRTInfo.attachments =
            {
                FramebufferAttachments{ "[Bloom Final]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT }
            };

            m_FinalRT = RenderTarget::Create(finalRTInfo, "[Bloom Final RT]");
        }
        else
        {
            m_FinalRT = nullptr;
        }

        InvalidatePipelines();
    }

    void Bloom::InvalidatePipelines()
    {
        m_DownsamplePipeline.reset();
        m_BlurPipeline.reset();
        m_UpsamplePipeline.reset();
    }

    void Bloom::EnsurePipelines()
    {
        if (!m_FinalRT || m_Levels.empty())
        {
            return;
        }

        if (m_DownsamplePipeline && m_BlurPipeline && m_UpsamplePipeline)
        {
            return;
        }

        IGN_PROFILE_SCOPE_COLOR("Bloom::EnsurePipelines", 0xFA0010FF);

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthTest = false;
        params.enableDepthWrite = false;
        params.cullMode = nri::CullMode::NONE;
        params.fillMode = nri::FillMode::SOLID;
        params.primitiveType = nri::Topology::TRIANGLE_LIST;

        auto buildPipeline = [&](const char *name, const Ref<Shader> &pixelShader, const BindingLayoutDesc &bindingLayout) -> Ref<GraphicsPipeline>
        {
            auto gp = GraphicsPipeline::Create(name);
            gp->SetShaders({ m_FullscreenVertexShader, pixelShader })
                .AddBindingLayout(bindingLayout)
                .Build(m_FinalRT.get(), params);
            return gp;
        };

        m_DownsamplePipeline = buildPipeline("Bloom Downsample", m_DownsamplePixelShader, m_DownsampleLayout);
        m_BlurPipeline = buildPipeline("Bloom Blur", m_BlurPixelShader, m_BlurLayout);
        m_UpsamplePipeline = buildPipeline("Bloom Upsample", m_UpsamplePixelShader, m_UpsampleLayout);
    }

    void Bloom::DrawFullscreen(nri::CommandBuffer *cmd, RenderTarget *renderTarget, const Ref<GraphicsPipeline> &pipeline,
        nri::DescriptorSet *bindingSet, const Ref<VertexBuffer> &fullscreenVertexBuffer) const
    {
        IGN_PROFILE_SCOPE_COLOR("Bloom::DrawFullscreen", 0xFA0010FF);

        if (!cmd || !renderTarget || !pipeline || !bindingSet || !fullscreenVertexBuffer)
            return;

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        renderTarget->BeginRendering(cmd, true, false);

        nri::Viewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(renderTarget->GetWidth());
        viewport.height = static_cast<float>(renderTarget->GetHeight());
        viewport.depthMin = 0.0f;
        viewport.depthMax = 1.0f;
        nri.CmdSetViewports(*cmd, &viewport, 1);

        nri::Rect scissor = {};
        scissor.x = 0;
        scissor.y = 0;
        scissor.width = static_cast<nri::Dim_t>(renderTarget->GetWidth());
        scissor.height = static_cast<nri::Dim_t>(renderTarget->GetHeight());
        nri.CmdSetScissors(*cmd, &scissor, 1);

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *pipeline->GetHandle());

        nri::SetDescriptorSetDesc setDesc = {};
        setDesc.setIndex = 0;
        setDesc.descriptorSet = bindingSet;
        setDesc.bindPoint = nri::BindPoint::GRAPHICS;
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::VertexBufferDesc vbDesc = {};
        vbDesc.buffer = fullscreenVertexBuffer->GetHandle();
        vbDesc.offset = 0;
        vbDesc.stride = sizeof(VertexScreen);
        nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);

        nri::DrawDesc drawDesc = {};
        drawDesc.vertexNum = 6;
        drawDesc.instanceNum = 1;
        nri.CmdDraw(*cmd, drawDesc);

        renderTarget->EndRendering(cmd);
    }

    void Bloom::Build(nri::CommandBuffer *cmd, const Ref<Texture> &sourceTexture, const Ref<VertexBuffer> &fullscreenVertexBuffer)
    {
        if (!cmd || !sourceTexture || !fullscreenVertexBuffer || m_Levels.empty() || !m_FinalRT)
        {
            return;
        }

        EnsurePipelines();
        if (!m_DownsamplePipeline || !m_BlurPipeline || !m_UpsamplePipeline)
        {
            return;
        }

        const auto maxLevels = static_cast<size_t>(std::clamp(settings.iterations, 1, static_cast<int>(m_Levels.size())));

        BlurParams horizontalParams;
        horizontalParams.horizontal = 1;
        horizontalParams.radius = settings.radius;
        m_BlurHorizontalParamsBuffer->SetData(&horizontalParams, sizeof(horizontalParams));

        BlurParams verticalParams;
        verticalParams.horizontal = 0;
        verticalParams.radius = settings.radius;
        m_BlurVerticalParamsBuffer->SetData(&verticalParams, sizeof(verticalParams));

        Ref<Texture> previousTexture = sourceTexture;

        // Downsample the source texture to create the bloom levels
        for (size_t i = 0; i < maxLevels; ++i)
        {
            Level &level = m_Levels[i];

            DownsampleParams params;
            params.threshold = (i == 0) ? settings.threshold : 0.0f;
            params.intensity = 1.0f;
            params.knee = settings.knee;
            level.downsampleParamsBuffer->SetData(&params, sizeof(params));

            BindingSetDesc desc;
            desc.AddItem(BindingSetItem::TextureSRV(0, previousTexture->GetShaderResourceView()));
            desc.AddItem(BindingSetItem::Sampler(0, m_Sampler));
            desc.AddItem(BindingSetItem::ConstantBuffer(0, level.downsampleParamsBuffer->GetDescriptor()));
            Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(desc, m_DownsamplePipeline->GetPipelineLayout(), 0);

            DrawFullscreen(cmd, level.downsampledRT.get(), m_DownsamplePipeline, bindingSet->GetHandle(), fullscreenVertexBuffer);

            previousTexture = level.downsampledRT->GetColorAttachment(0);
        }

        // Apply horizontal and vertical blur to each level
        for (size_t i = 0; i < maxLevels; ++i)
        {
            Level &level = m_Levels[i];

            BindingSetDesc hDesc;
            hDesc.AddItem(BindingSetItem::TextureSRV(0, level.downsampledRT->GetColorAttachment(0)->GetShaderResourceView()));
            hDesc.AddItem(BindingSetItem::Sampler(0, m_Sampler));
            hDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_BlurHorizontalParamsBuffer->GetDescriptor()));
            Ref<BindingSet> hBindingSet = BindingCache::GetOrCreateBindingSet(hDesc, m_BlurPipeline->GetPipelineLayout(), 0);

            DrawFullscreen(cmd, level.blurHorizontalRT.get(), m_BlurPipeline, hBindingSet->GetHandle(), fullscreenVertexBuffer);

            BindingSetDesc vDesc;
            vDesc.AddItem(BindingSetItem::TextureSRV(0, level.blurHorizontalRT->GetColorAttachment(0)->GetShaderResourceView()));
            vDesc.AddItem(BindingSetItem::Sampler(0, m_Sampler));
            vDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_BlurVerticalParamsBuffer->GetDescriptor()));
            Ref<BindingSet> vBindingSet = BindingCache::GetOrCreateBindingSet(vDesc, m_BlurPipeline->GetPipelineLayout(), 0);

            DrawFullscreen(cmd, level.blurVerticalRT.get(), m_BlurPipeline, vBindingSet->GetHandle(), fullscreenVertexBuffer);
        }

        // Upsample and combine the blurred levels back together
        Ref<Texture> currentTexture = m_Levels[maxLevels - 1].blurVerticalRT->GetColorAttachment(0);
        for (int i = static_cast<int>(maxLevels) - 2; i >= 0; --i)
        {
            Level &level = m_Levels[static_cast<size_t>(i)];
            Ref<RenderTarget> output = (i == 0) ? m_FinalRT : level.upsampledRT;

            UpsampleParams params;
            params.radius = settings.radius * static_cast<float>(i + 1);
            level.upsampleParamsBuffer->SetData(&params, sizeof(params));

            BindingSetDesc desc;
            desc.AddItem(BindingSetItem::TextureSRV(0, currentTexture->GetShaderResourceView()));
            desc.AddItem(BindingSetItem::TextureSRV(1, level.blurVerticalRT->GetColorAttachment(0)->GetShaderResourceView()));
            desc.AddItem(BindingSetItem::Sampler(0, m_Sampler));
            desc.AddItem(BindingSetItem::ConstantBuffer(0, level.upsampleParamsBuffer->GetDescriptor()));
            Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(desc, m_UpsamplePipeline->GetPipelineLayout(), 0);

            DrawFullscreen(cmd, output.get(), m_UpsamplePipeline, bindingSet->GetHandle(), fullscreenVertexBuffer);

            currentTexture = output->GetColorAttachment(0);
        }

        if (maxLevels == 1)
        {
            UpsampleParams params;
            params.radius = settings.radius;
            m_SingleLevelUpsampleParamsBuffer->SetData(&params, sizeof(params));

            BindingSetDesc desc;
            desc.AddItem(BindingSetItem::TextureSRV(0, currentTexture->GetShaderResourceView()));
            desc.AddItem(BindingSetItem::TextureSRV(1, Renderer::GetBlackTexture()->GetShaderResourceView()));
            desc.AddItem(BindingSetItem::Sampler(0, m_Sampler));
            desc.AddItem(BindingSetItem::ConstantBuffer(0, m_SingleLevelUpsampleParamsBuffer->GetDescriptor()));
            Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(desc, m_UpsamplePipeline->GetPipelineLayout(), 0);

            DrawFullscreen(cmd, m_FinalRT.get(), m_UpsamplePipeline, bindingSet->GetHandle(), fullscreenVertexBuffer);
        }
    }

    void Bloom::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || (m_Width == width && m_Height == height))
            return;
        CreateRenderTargets(width, height);
    }

    Ref<Texture> Bloom::GetBloomTexture() const
    {
        if (!m_FinalRT)
        {
            return nullptr;
        }

        return m_FinalRT->GetColorAttachment(0);
    }
}
