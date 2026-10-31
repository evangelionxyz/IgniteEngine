// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GRAPHICS_PIPELINE_HPP
#define IGN_GRAPHICS_PIPELINE_HPP

#include "ignite/core/types.hpp"
#include "shader.hpp"
#include "binding_set.hpp"
#include "hash_keys.hpp"

#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include <NRI.h>
#include <vector>
#include <unordered_map>

namespace ignite
{
    class RenderTarget;

    struct GraphicsPipelineParams
    {
        nri::CullMode cullMode = nri::CullMode::FRONT;
        nri::Topology primitiveType = nri::Topology::TRIANGLE_LIST;
        nri::FillMode fillMode = nri::FillMode::SOLID;

        nri::BlendFactor srcBlend = nri::BlendFactor::ONE;
        nri::BlendFactor destBlend = nri::BlendFactor::ZERO;
        nri::BlendFactor srcBlendAlpha = nri::BlendFactor::ONE;
        nri::BlendFactor destBlendAlpha = nri::BlendFactor::ZERO;
        nri::BlendOp blendOp = nri::BlendOp::ADD;
        nri::BlendOp blendOpAlpha = nri::BlendOp::ADD;

        nri::StencilDesc frontFaceStencilDesc = {};
        nri::StencilDesc backFaceStencilDesc = {};
        nri::CompareOp depthFunc = nri::CompareOp::ALWAYS;

        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;
        uint8_t stencilRefValue = 0;

        bool enableBlend = true;
        bool enableDepthStencil = false;
        bool enableDepthWrite = false;
        bool enableDepthTest = false;

        bool enableScissor = false;
        bool enableDepthClip = false;

        nri::DepthBiasDesc depthBias = {};

        nri::ColorWriteBits colorWriteMask[8] = {
            nri::ColorWriteBits::RGBA, nri::ColorWriteBits::RGBA,
            nri::ColorWriteBits::RGBA, nri::ColorWriteBits::RGBA,
            nri::ColorWriteBits::RGBA, nri::ColorWriteBits::RGBA,
            nri::ColorWriteBits::RGBA, nri::ColorWriteBits::RGBA
        };
    };

    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const std::string &name = "Graphics Pipeline");
        ~GraphicsPipeline();

        const std::string &GetName() const { return m_Name; }

        GraphicsPipeline &AddBindingLayout(const BindingLayoutDesc &layout);
        GraphicsPipeline &SetShaders(const std::vector<Ref<Shader>> &shaders, bool recompile = false);
        void Build(RenderTarget *framebuffer, const GraphicsPipelineParams &params);
        void Build(const FramebufferKey &framebufferInfo, const GraphicsPipelineParams &params);

        const BindingLayoutDesc &GetBindingLayout(uint32_t index) const;

        nri::Pipeline *GetHandle() const { return m_Handle; }
        nri::PipelineLayout *GetPipelineLayout() const { return m_PipelineLayout; }
        operator nri::Pipeline *() const { return m_Handle; }

        Ref<Shader> GetShader(UMBRA_ShaderType shaderType)
        {
            auto it = std::find_if(m_Shaders.begin(), m_Shaders.end(), [shaderType](Ref<Shader> shader)
            {
                return shader->GetType() == shaderType;
            });

            if (it != m_Shaders.end())
            {
                return *it;
            }

            return nullptr;
        }

        static Ref<GraphicsPipeline> Create(const std::string &name = "Graphics Pipeline");

        GraphicsPipelineParams &GetParams() { return m_Params; }
        const GraphicsPipelineParams &GetParams() const { return m_Params; }

    private:
        void ReleasePipelineResources();
        void UnsubscribeShaders();
        void OnShaderReload();

    private:
        struct ShaderListener
        {
            Ref<Shader> shader;
            ShaderReloadToken token = 0;
        };

        std::string m_Name;
        nri::Pipeline *m_Handle = nullptr;
        nri::PipelineLayout *m_PipelineLayout = nullptr;
        std::vector<Ref<Shader>> m_Shaders;
        std::vector<ShaderListener> m_ShaderListeners;

        // Lets queued render-thread rebuilds detect that this pipeline is gone.
        std::shared_ptr<bool> m_Alive = std::make_shared<bool>(true);

        std::vector<BindingLayoutDesc> m_BindingLayouts;

        GraphicsPipelineParams m_Params;
        // Copy of the format info (not the framebuffer pointer, which may be destroyed/recreated, e.g. on resize)
        FramebufferKey m_FramebufferInfo;
        bool m_HasFramebufferInfo = false;
    };
}

#endif
