// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "graphics_pipeline.hpp"
#include "render_target.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "gpu_upload_sync.hpp"
#include "shader.hpp"

#include "renderer.hpp"

namespace ignite
{
    namespace
    {
        bool IsIntegerFormat(nri::Format format)
        {
            switch (format)
            {
                case nri::Format::R8_UINT:
                case nri::Format::R8_SINT:
                case nri::Format::RG8_UINT:
                case nri::Format::RG8_SINT:
                case nri::Format::RGBA8_UINT:
                case nri::Format::RGBA8_SINT:
                case nri::Format::R16_UINT:
                case nri::Format::R16_SINT:
                case nri::Format::RG16_UINT:
                case nri::Format::RG16_SINT:
                case nri::Format::RGBA16_UINT:
                case nri::Format::RGBA16_SINT:
                case nri::Format::R32_UINT:
                case nri::Format::R32_SINT:
                case nri::Format::RG32_UINT:
                case nri::Format::RG32_SINT:
                case nri::Format::RGB32_UINT:
                case nri::Format::RGB32_SINT:
                case nri::Format::RGBA32_UINT:
                case nri::Format::RGBA32_SINT:
                case nri::Format::R10_G10_B10_A2_UINT:
                    return true;
                default:
                    return false;
            }
        }
    }

    GraphicsPipeline &GraphicsPipeline::AddBindingLayout(const BindingLayoutDesc &layout)
    {
        m_BindingLayouts.emplace_back(layout);
        return *this;
    }

    GraphicsPipeline &GraphicsPipeline::SetShaders(const std::vector<Ref<Shader>> &shaders, bool recompile)
    {
        (void)recompile;
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

    const BindingLayoutDesc &GraphicsPipeline::GetBindingLayout(uint32_t index) const
    {
        static const BindingLayoutDesc s_EmptyLayout = {};
        if (index < m_BindingLayouts.size())
            return m_BindingLayouts[index];
        return s_EmptyLayout;
    }

    void GraphicsPipeline::Build(RenderTarget *framebuffer, const GraphicsPipelineParams &params)
    {
        LOG_ASSERT(framebuffer, "[GraphicsPipeline] Framebuffer is null");
        Build(MakeFramebufferKey(framebuffer, params.fillMode, params.cullMode), params);
    }

    void GraphicsPipeline::Build(const FramebufferKey &framebufferInfo, const GraphicsPipelineParams &params)
    {
        m_FramebufferInfo = framebufferInfo;
        m_HasFramebufferInfo = true;
        m_Params = params;

        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm ? dm->GetDevice() : nullptr;
        LOG_ASSERT(device, "[GraphicsPipeline] Device is null");

        ReleasePipelineResources();

        m_PipelineLayout = BindingLayoutDesc::CreatePipelineLayout(m_BindingLayouts, nri::StageBits::ALL, device);
        LOG_ASSERT(m_PipelineLayout, "[GraphicsPipeline] Failed to create pipeline layout for '{}'", m_Name);

        nri::GraphicsPipelineDesc pipelineDesc = {};
        pipelineDesc.pipelineLayout = m_PipelineLayout;

        nri::VertexInputDesc vertexInputDesc = {};
        std::vector<nri::ShaderDesc> shaderDescs;
        shaderDescs.reserve(m_Shaders.size());

        for (const Ref<Shader> &shader : m_Shaders)
        {
            if (!shader)
                continue;

            shaderDescs.push_back(shader->GetShaderDesc());

            if (shader->GetType() == UMBRA_SHADER_TYPE_VERTEX)
            {
                const auto &vertexAttributes = shader->GetVertexAttributes();
                const auto &vertexStreams = shader->GetVertexStreams();
                if (!vertexAttributes.empty())
                {
                    vertexInputDesc.attributes = vertexAttributes.data();
                    vertexInputDesc.attributeNum = static_cast<uint8_t>(vertexAttributes.size());
                    vertexInputDesc.streams = vertexStreams.data();
                    vertexInputDesc.streamNum = static_cast<uint8_t>(vertexStreams.size());
                    pipelineDesc.vertexInput = &vertexInputDesc;
                }
            }
        }

        pipelineDesc.shaders = shaderDescs.data();
        pipelineDesc.shaderNum = static_cast<uint32_t>(shaderDescs.size());

        pipelineDesc.inputAssembly.topology = m_Params.primitiveType;

        pipelineDesc.rasterization.fillMode = m_Params.fillMode;
        pipelineDesc.rasterization.cullMode = m_Params.cullMode;
        pipelineDesc.rasterization.frontCounterClockwise = false;
        pipelineDesc.rasterization.depthClamp = !m_Params.enableDepthClip;
        pipelineDesc.rasterization.depthBias = m_Params.depthBias;

        nri::MultisampleDesc multisampleDesc = {};
        multisampleDesc.sampleNum = static_cast<nri::Sample_t>(std::max<uint32_t>(1u, m_FramebufferInfo.sampleCount));
        multisampleDesc.sampleMask = nri::ALL;
        if (m_FramebufferInfo.sampleCount > 1)
        {
            pipelineDesc.multisample = &multisampleDesc;
        }

        std::vector<nri::ColorAttachmentDesc> colorAttachments(m_FramebufferInfo.colorFormats.size());
        for (size_t ti = 0; ti < colorAttachments.size(); ++ti)
        {
            colorAttachments[ti].format = m_FramebufferInfo.colorFormats[ti];
            colorAttachments[ti].colorWriteMask = (ti < 8) ? m_Params.colorWriteMask[ti] : nri::ColorWriteBits::RGBA;

            if (ti == 0 && !IsIntegerFormat(m_FramebufferInfo.colorFormats[0]))
            {
                colorAttachments[ti].blendEnabled = m_Params.enableBlend;
                colorAttachments[ti].colorBlend = { m_Params.srcBlend, m_Params.destBlend, m_Params.blendOp };
                colorAttachments[ti].alphaBlend = { m_Params.srcBlendAlpha, m_Params.destBlendAlpha, m_Params.blendOpAlpha };
            }
            else
            {
                colorAttachments[ti].blendEnabled = false;
            }
        }

        pipelineDesc.outputMerger.colors = colorAttachments.empty() ? nullptr : colorAttachments.data();
        pipelineDesc.outputMerger.colorNum = static_cast<uint32_t>(colorAttachments.size());
        pipelineDesc.outputMerger.depthStencilFormat = m_FramebufferInfo.depthFormat;

        if (m_FramebufferInfo.depthFormat != nri::Format::UNKNOWN)
        {
            pipelineDesc.outputMerger.depth.write = m_Params.enableDepthWrite;
            pipelineDesc.outputMerger.depth.compareOp = m_Params.enableDepthTest ? m_Params.depthFunc : nri::CompareOp::NONE;

            if (m_Params.enableDepthStencil)
            {
                pipelineDesc.outputMerger.stencil.front = m_Params.frontFaceStencilDesc;
                pipelineDesc.outputMerger.stencil.back = m_Params.backFaceStencilDesc;
                pipelineDesc.outputMerger.stencil.front.compareMask = m_Params.stencilReadMask;
                pipelineDesc.outputMerger.stencil.front.writeMask = m_Params.stencilWriteMask;
                pipelineDesc.outputMerger.stencil.back.compareMask = m_Params.stencilReadMask;
                pipelineDesc.outputMerger.stencil.back.writeMask = m_Params.stencilWriteMask;
            }
        }

        const auto &nri = dm->GetCoreInterface();
        nri::Result res = nri.CreateGraphicsPipeline(*device, pipelineDesc, m_Handle);
        LOG_ASSERT(res == nri::Result::SUCCESS && m_Handle, "[GraphicsPipeline] Failed to create graphics pipeline '{}'", m_Name);
        if (m_Handle)
        {
            nri.SetDebugName(m_Handle, m_Name.c_str());
        }
    }

    GraphicsPipeline::GraphicsPipeline(const std::string &name)
        : m_Name(name)
    {
        Renderer::IncrementPipelineCount(m_Name);
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        UnsubscribeShaders();
        ReleasePipelineResources();
        Renderer::DecrementPipelineCount(m_Name);
    }

    void GraphicsPipeline::ReleasePipelineResources()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;

        const auto &nri = dm->GetCoreInterface();
        if (m_Handle)
        {
            nri.DestroyPipeline(m_Handle);
            m_Handle = nullptr;
        }
        if (m_PipelineLayout)
        {
            nri.DestroyPipelineLayout(m_PipelineLayout);
            m_PipelineLayout = nullptr;
        }
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create(const std::string &name)
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

            nri::Device *device = DeviceManager::GetInstance()->GetDevice();
            GPUUploadSync::DeviceWaitIdle(device);
            ReleasePipelineResources();

            // Rebuild with the same framebuffer and params
            Build(m_FramebufferInfo, m_Params);
        }, "GraphicsPipeline::OnShaderReload");
    }
}
