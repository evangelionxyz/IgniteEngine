// Copyright (c) 2026 Evangelion Manuhutu

#include "bloom.hpp"

#include "buffers/vertex_buffer.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "renderer.hpp"

#include "ignite/core/device/device_manager.hpp"

#include <algorithm>

namespace ignite
{
    Bloom::Bloom(int width, int height)
        : m_Width(std::max(width, 1)), m_Height(std::max(height, 1))
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        nvrhi::BindingLayoutDesc downsampleLayout;
        downsampleLayout.visibility = nvrhi::ShaderType::All;
        downsampleLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
        downsampleLayout.addItem(nvrhi::BindingLayoutItem::Sampler(0));
        downsampleLayout.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
        m_DownsampleLayout = device->createBindingLayout(downsampleLayout);

        nvrhi::BindingLayoutDesc blurLayout;
        blurLayout.visibility = nvrhi::ShaderType::All;
        blurLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
        blurLayout.addItem(nvrhi::BindingLayoutItem::Sampler(0));
        blurLayout.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
        m_BlurLayout = device->createBindingLayout(blurLayout);

        nvrhi::BindingLayoutDesc upsampleLayout;
        upsampleLayout.visibility = nvrhi::ShaderType::All;
        upsampleLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
        upsampleLayout.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
        upsampleLayout.addItem(nvrhi::BindingLayoutItem::Sampler(0));
        upsampleLayout.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
        m_UpsampleLayout = device->createBindingLayout(upsampleLayout);

        auto samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        samplerDesc.setAllFilters(true);
        m_Sampler = device->createSampler(samplerDesc);

        constexpr uint32_t kBloomCBVersions = 512;
        m_DownsampleParamsBuffer = ConstantBuffer::Create(sizeof(DownsampleParams), true, kBloomCBVersions, "Bloom Downsample Params");
        m_BlurParamsBuffer = ConstantBuffer::Create(sizeof(BlurParams), true, kBloomCBVersions, "Bloom Blur Params");
        m_UpsampleParamsBuffer = ConstantBuffer::Create(sizeof(UpsampleParams), true, kBloomCBVersions, "Bloom Upsample Params");

        m_FullscreenVertexShader = Shader::Create("resources/shaders/bloom_fullscreen.vertex.hlsl", ShaderType::Vertex, true);
        m_DownsamplePixelShader = Shader::Create("resources/shaders/bloom_downsample.pixel.hlsl", ShaderType::Pixel, true);
        m_BlurPixelShader = Shader::Create("resources/shaders/bloom_blur.pixel.hlsl", ShaderType::Pixel, true);
        m_UpsamplePixelShader = Shader::Create("resources/shaders/bloom_upsample.pixel.hlsl", ShaderType::Pixel, true);

        CreateRenderTargets(m_Width, m_Height);
    }

    Bloom::~Bloom()
    {
        m_DownsamplePipeline = nullptr;
        m_BlurPipeline = nullptr;
        m_UpsamplePipeline = nullptr;
        m_InputLayout = nullptr;
        m_Sampler = nullptr;
    }

    void Bloom::CreateRenderTargets(uint32_t width, uint32_t height)
    {
        m_Width = std::max(width, 1u);
        m_Height = std::max(height, 1u);

        m_Levels.clear();

        uint32_t w = std::max(m_Width / 2, 1u);
        uint32_t h = std::max(m_Height / 2, 1u);

        RenderTargetCreateInfo levelRTInfo;
        levelRTInfo.attachments =
        {
            FramebufferAttachments{ "[Bloom Color]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget }
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
            level.blurHorizontalRT = RenderTarget::Create(levelRTInfo, "[Bloom Blur Horizontal RT]");
            level.blurVerticalRT = RenderTarget::Create(levelRTInfo, "[Bloom Blur Vertical RT]");

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
                FramebufferAttachments{ "[Bloom Final]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget }
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
        m_DownsamplePipeline = nullptr;
        m_BlurPipeline = nullptr;
        m_UpsamplePipeline = nullptr;
        m_InputLayout = nullptr;
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

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        if (!m_InputLayout)
        {
            const auto &vertexAttributes = m_FullscreenVertexShader->GetVertexAttributes();
            m_InputLayout = device->createInputLayout(vertexAttributes.data(), static_cast<uint32_t>(vertexAttributes.size()), nullptr);
        }

        nvrhi::RenderState renderState;
        renderState.blendState.targets[0].blendEnable = false;
        renderState.depthStencilState.depthTestEnable = false;
        renderState.depthStencilState.depthWriteEnable = false;
        renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
        renderState.rasterState.fillMode = nvrhi::RasterFillMode::Solid;

        auto buildPipeline = [&](Ref<Shader> pixelShader, nvrhi::BindingLayoutHandle bindingLayout) -> nvrhi::GraphicsPipelineHandle
        {
            nvrhi::GraphicsPipelineDesc desc;
            desc.setVertexShader(m_FullscreenVertexShader->GetHandle());
            desc.setPixelShader(pixelShader->GetHandle());
            desc.setInputLayout(m_InputLayout);
            desc.addBindingLayout(bindingLayout);
            desc.setRenderState(renderState);
            desc.primType = nvrhi::PrimitiveType::TriangleList;

            return device->createGraphicsPipeline(desc, m_FinalRT->GetFramebuffer()->getFramebufferInfo());
        };

        m_DownsamplePipeline = buildPipeline(m_DownsamplePixelShader, m_DownsampleLayout);
        m_BlurPipeline = buildPipeline(m_BlurPixelShader, m_BlurLayout);
        m_UpsamplePipeline = buildPipeline(m_UpsamplePixelShader, m_UpsampleLayout);
    }

    void Bloom::DrawFullscreen(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, nvrhi::GraphicsPipelineHandle pipeline,
        nvrhi::BindingSetHandle bindingSet, const Ref<VertexBuffer> &fullscreenVertexBuffer) const
    {
        nvrhi::GraphicsState state;
        state.pipeline = pipeline;
        state.framebuffer = framebuffer;
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        state.vertexBuffers = { nvrhi::VertexBufferBinding{ fullscreenVertexBuffer->GetHandle(), 0, 0 } };
        state.bindings = { bindingSet };

        cmd->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.instanceCount = 1;
        args.vertexCount = 6;
        cmd->draw(args);
    }

    void Bloom::Build(nvrhi::ICommandList *cmd, const Ref<Texture> &sourceTexture, const Ref<VertexBuffer> &fullscreenVertexBuffer)
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

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        const size_t maxLevels = static_cast<size_t>(std::clamp(settings.iterations, 1, static_cast<int>(m_Levels.size())));

        Ref<Texture> previousTexture = sourceTexture;

        for (size_t i = 0; i < maxLevels; ++i)
        {
            Level &level = m_Levels[i];
            level.downsampledRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));

            DownsampleParams params;
            params.threshold = (i == 0) ? settings.threshold : 0.0f;
            params.intensity = settings.intensity;
            params.knee = settings.knee;
            m_DownsampleParamsBuffer->SetData(cmd, Buffer(&params, sizeof(params)));

            auto desc = nvrhi::BindingSetDesc();
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, previousTexture->GetHandle()));
            desc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_DownsampleParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle bindingSet = device->createBindingSet(desc, m_DownsampleLayout);

            DrawFullscreen(cmd, level.downsampledRT->GetFramebuffer(), m_DownsamplePipeline, bindingSet, fullscreenVertexBuffer);
            previousTexture = level.downsampledRT->GetColorAttachment(0);
        }

        for (size_t i = 0; i < maxLevels; ++i)
        {
            Level &level = m_Levels[i];

            level.blurHorizontalRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
            BlurParams horizontalParams;
            horizontalParams.horizontal = 1;
            m_BlurParamsBuffer->SetData(cmd, Buffer(&horizontalParams, sizeof(horizontalParams)));

            auto hDesc = nvrhi::BindingSetDesc();
            hDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, level.downsampledRT->GetColorAttachment(0)->GetHandle()));
            hDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));
            hDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_BlurParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle hBindingSet = device->createBindingSet(hDesc, m_BlurLayout);

            DrawFullscreen(cmd, level.blurHorizontalRT->GetFramebuffer(), m_BlurPipeline, hBindingSet, fullscreenVertexBuffer);

            level.blurVerticalRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
            BlurParams verticalParams;
            verticalParams.horizontal = 0;
            m_BlurParamsBuffer->SetData(cmd, Buffer(&verticalParams, sizeof(verticalParams)));

            auto vDesc = nvrhi::BindingSetDesc();
            vDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, level.blurHorizontalRT->GetColorAttachment(0)->GetHandle()));
            vDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));
            vDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_BlurParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle vBindingSet = device->createBindingSet(vDesc, m_BlurLayout);

            DrawFullscreen(cmd, level.blurVerticalRT->GetFramebuffer(), m_BlurPipeline, vBindingSet, fullscreenVertexBuffer);
        }

        Ref<Texture> currentTexture = m_Levels[maxLevels - 1].blurVerticalRT->GetColorAttachment(0);
        for (int i = static_cast<int>(maxLevels) - 2; i >= 0; --i)
        {
            Level &level = m_Levels[static_cast<size_t>(i)];
            Ref<RenderTarget> output = (i == 0) ? m_FinalRT : level.blurHorizontalRT;
            output->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));

            UpsampleParams params;
            params.radius = settings.radius * static_cast<float>(i + 1);
            m_UpsampleParamsBuffer->SetData(cmd, Buffer(&params, sizeof(params)));

            auto desc = nvrhi::BindingSetDesc();
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, currentTexture->GetHandle()));
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, level.blurVerticalRT->GetColorAttachment(0)->GetHandle()));
            desc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_UpsampleParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle bindingSet = device->createBindingSet(desc, m_UpsampleLayout);

            DrawFullscreen(cmd, output->GetFramebuffer(), m_UpsamplePipeline, bindingSet, fullscreenVertexBuffer);
            currentTexture = output->GetColorAttachment(0);
        }

        if (maxLevels == 1)
        {
            m_FinalRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));

            UpsampleParams params;
            params.radius = settings.radius;
            m_UpsampleParamsBuffer->SetData(cmd, Buffer(&params, sizeof(params)));

            auto desc = nvrhi::BindingSetDesc();
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, currentTexture->GetHandle()));
            desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, Renderer::GetBlackTexture()->GetHandle()));
            desc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_UpsampleParamsBuffer->GetHandle()));
            nvrhi::BindingSetHandle bindingSet = device->createBindingSet(desc, m_UpsampleLayout);

            DrawFullscreen(cmd, m_FinalRT->GetFramebuffer(), m_UpsamplePipeline, bindingSet, fullscreenVertexBuffer);
        }
    }

    void Bloom::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        if (m_Width == width && m_Height == height)
        {
            return;
        }

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
