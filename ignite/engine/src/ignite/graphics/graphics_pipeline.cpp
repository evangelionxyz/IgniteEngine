// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "graphics_pipeline.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "gpu_upload_sync.hpp"
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
        UnsubscribeShaders();
        m_Shaders = shaders;

        // Listen to each shader directly, so only the pipelines using a changed shader rebuild.
        m_ShaderListeners.reserve(m_Shaders.size());
        for (const Ref<Shader> &shader : m_Shaders)
        {
            if (!shader)
                continue;

            const ShaderReloadToken token = shader->AddReloadListener([this](Shader &) { OnShaderReload(); });
            m_ShaderListeners.push_back({ shader, token });
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
        LOG_ASSERT(framebuffer, "[GraphicsPipeline] Framebuffer is null");
        Build(framebuffer->getFramebufferInfo(), params);
    }

    void GraphicsPipeline::Build(const nvrhi::FramebufferInfoEx &framebufferInfo, const GraphicsPipelineParams &params)
    {
        m_FramebufferInfo = framebufferInfo;
        m_HasFramebufferInfo = true;

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
        renderState.rasterState.multisampleEnable = m_FramebufferInfo.sampleCount > 1;

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
        m_Handle = device->createGraphicsPipeline(pipelineDesc, m_FramebufferInfo);
        LOG_ASSERT(m_Handle, "Failed to create graphics pipeline");
    }

    GraphicsPipeline::GraphicsPipeline(const std::string& name)
        : m_Name(name)
    {
        Renderer::IncrementPipelineCount(m_Name);
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        UnsubscribeShaders();
        Renderer::DecrementPipelineCount(m_Name);
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create(const std::string& name)
    {
        return CreateRef<GraphicsPipeline>(name);
    }

    void GraphicsPipeline::UnsubscribeShaders()
    {
        for (ShaderListener &listener : m_ShaderListeners)
        {
            if (listener.shader)
                listener.shader->RemoveReloadListener(listener.token);
        }
        m_ShaderListeners.clear();
    }

    void GraphicsPipeline::OnShaderReload()
    {
        // Never built yet: the new shader will be picked up by the first Build().
        if (!m_Handle || !m_HasFramebufferInfo)
            return;

        // The render thread owns PSO usage, so swap the PSO there to avoid racing a frame in flight.
        std::weak_ptr<bool> alive = m_Alive;
        Application::SubmitToRenderThread([this, alive]()
        {
            if (alive.expired())
                return;

            LOG_INFO("[GraphicsPipeline] Rebuilding '{}' after shader reload", m_Name);

            nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
            GPUUploadSync::DeviceWaitIdle(device);
            m_Handle = nullptr;

            // Rebuild with the same framebuffer and params
            Build(m_FramebufferInfo, m_Params);
        }, "GraphicsPipeline::OnShaderReload");
    }
}
