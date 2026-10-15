// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "graphics_pipeline.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "shader.hpp"

#include "renderer.hpp"

namespace ignite
{
    GraphicsPipeline& GraphicsPipeline::AddBindingLayout(const nvrhi::BindingLayoutHandle& layout)
    {
        m_BindingLayouts.emplace_back(layout);
        return *this;
    }

    GraphicsPipeline& GraphicsPipeline::SetShaders(const std::vector<Ref<Shader>> &shaders, bool recompile)
    {
        m_Shaders = shaders;

        // Unsubscribe first (on shot only)
        for (SignalToken token : m_ShaderSignalTokens)
        {
            SignalBus::Unsubscribe<AssetChangeSignal>(token);
        }

        m_ShaderSignalTokens.resize(m_Shaders.size());
        for (size_t i = 0; i < m_Shaders.size(); ++i)
        {
            m_ShaderSignalTokens[i] = SignalBus::Subscribe<AssetChangeSignal>(
                [this](const AssetChangeSignal &signal)
                {
                    // if (signal.type == AssetType::Shader)
                    //     OnShaderReload(signal);
                }
            );
        }
        return *this;
    }

    nvrhi::BindingLayoutHandle GraphicsPipeline::GetBindingLayout(uint32_t index)
    {
        if (index < m_BindingLayouts.size())
            return m_BindingLayouts[index];
        return nullptr;
    }

    void GraphicsPipeline::Build(nvrhi::IFramebuffer *framebuffer, const GraphicsPipelineParams &params)
    {
        m_FramebufferHandle = framebuffer;

        nvrhi::IDevice* device = DeviceManager::GetInstance()->GetDevice();

        LOG_ASSERT(m_Handle == nullptr, "[GraphicsPipeline] Should not re-create pipeline");

        m_Params = params;

        // create graphics pipeline
        nvrhi::RenderState renderState;
        for (int ti = 0; ti < 8; ++ti)
        {
            renderState.blendState.targets[ti].colorWriteMask = m_Params.colorWriteMask[ti];
        }
        renderState.blendState.targets[0].blendEnable = m_Params.enableBlend;
        renderState.blendState.targets[0].srcBlend = m_Params.srcBlend;
        renderState.blendState.targets[0].destBlend = m_Params.destBlend;
        renderState.blendState.targets[0].srcBlendAlpha = m_Params.srcBlendAlpha;
        renderState.blendState.targets[0].destBlendAlpha = m_Params.destBlendAlpha;

        renderState.depthStencilState.depthWriteEnable = m_Params.enableDepthWrite;
        renderState.depthStencilState.depthTestEnable = m_Params.enableDepthTest;
        renderState.depthStencilState.depthFunc = m_Params.depthFunc;
        renderState.depthStencilState.stencilEnable = m_Params.enableDepthStencil;
        renderState.depthStencilState.frontFaceStencil = m_Params.frontFaceStencilDesc;
        renderState.depthStencilState.backFaceStencil = m_Params.backFaceStencilDesc;
        renderState.depthStencilState.stencilWriteMask = m_Params.stencilWriteMask;
        renderState.depthStencilState.stencilReadMask = m_Params.stencilReadMask;
        renderState.depthStencilState.stencilRefValue = m_Params.stencilRefValue;

        renderState.rasterState.cullMode = m_Params.cullMode;
        renderState.rasterState.fillMode = m_Params.fillMode;
        renderState.rasterState.scissorEnable = m_Params.enableScissor;
        renderState.rasterState.depthClipEnable = m_Params.enableDepthClip;
        renderState.rasterState.frontCounterClockwise = false;
        renderState.rasterState.multisampleEnable = framebuffer && (framebuffer->getFramebufferInfo().sampleCount > 1);

        nvrhi::GraphicsPipelineDesc pipelineDesc;

        for (const Ref<Shader> &shader : m_Shaders)
        {
            if (shader->GetType() == UMBRA_SHADER_TYPE_VERTEX)
            {
                pipelineDesc.setVertexShader(shader->GetHandle());

				const auto &vertexAttributes = shader->GetVertexAttributes();
                m_InputLayout = device->createInputLayout(vertexAttributes.data(), static_cast<uint32_t>(vertexAttributes.size()), nullptr);
                LOG_ASSERT(m_InputLayout, "[Graphics Pipeline] Failed to create input layout");
            }
            else if (shader->GetType() == UMBRA_SHADER_TYPE_PIXEL)
            {
                pipelineDesc.setPixelShader(shader->GetHandle());
            }
        }

        if (m_InputLayout)
        {
            pipelineDesc.setInputLayout(m_InputLayout);
        }

        pipelineDesc.setRenderState(renderState);
        pipelineDesc.primType = m_Params.primitiveType;

        for (auto& layout : m_BindingLayouts)
        {
            pipelineDesc.addBindingLayout(layout);
        }

        // create with the same framebuffer to be rendered
        m_Handle = device->createGraphicsPipeline(pipelineDesc, framebuffer->getFramebufferInfo());
        LOG_ASSERT(m_Handle, "Failed to create graphics pipeline");
    }

    GraphicsPipeline::GraphicsPipeline(const std::string& name)
        : m_Name(name)
    {
        Renderer::IncrementPipelineCount(m_Name);
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        Renderer::DecrementPipelineCount(m_Name);
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create(const std::string& name)
    {
        return CreateRef<GraphicsPipeline>(name);
    }

    void GraphicsPipeline::OnShaderReload(const AssetChangeSignal &signal)
    {
        // Reset
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        GPUUploadSync::DeviceWaitIdle(device);
        m_Handle = nullptr;
        Build(m_FramebufferHandle, m_Params);
    }
}
