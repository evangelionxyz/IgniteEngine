// Copyright (c) 2026 Evangelion Manuhutu 

#pragma once
#ifndef IGN_GRAPHICS_PIPELINE_HPP
#define IGN_GRAPHICS_PIPELINE_HPP

#include "ignite/core/types.hpp"
#include "shader.hpp"

#include <nvrhi/nvrhi.h>
#include <vector>
#include <unordered_map>

namespace ignite {

    struct GraphicsPipelineParams
    {
        nvrhi::RasterCullMode cullMode = nvrhi::RasterCullMode::Front;
        nvrhi::PrimitiveType primitiveType = nvrhi::PrimitiveType::TriangleList;
        nvrhi::RasterFillMode fillMode = nvrhi::RasterFillMode::Solid;

        nvrhi::BlendFactor srcBlend = nvrhi::BlendFactor::One;
        nvrhi::BlendFactor destBlend = nvrhi::BlendFactor::Zero;
        nvrhi::BlendFactor srcBlendAlpha = nvrhi::BlendFactor::One;
        nvrhi::BlendFactor destBlendAlpha = nvrhi::BlendFactor::Zero;

        nvrhi::DepthStencilState::StencilOpDesc frontFaceStencilDesc;
        nvrhi::DepthStencilState::StencilOpDesc backFaceStencilDesc;
        nvrhi::ComparisonFunc depthFunc = nvrhi::ComparisonFunc::Always;

        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;
        uint8_t stencilRefValue = 0;

        bool enableBlend = true;
        bool enableDepthStencil = false;
        bool enableDepthWrite = false;
        bool enableDepthTest = false;

        bool enableScissor = false;
        bool enableDepthClip = false;
    };

    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const std::string& name = "Graphics Pipeline");
        ~GraphicsPipeline();

        const std::string& GetName() const { return m_Name; }

        GraphicsPipeline &AddBindingLayout(const nvrhi::BindingLayoutHandle &layout);
        GraphicsPipeline &SetShaders(const std::vector<Ref<Shader>> &shaders, bool recompile = false);
        void Build(nvrhi::IFramebuffer *framebuffer, const GraphicsPipelineParams &params);

        nvrhi::BindingLayoutHandle GetBindingLayout(uint32_t index);

        nvrhi::GraphicsPipelineHandle GetHandle() { return m_Handle; }
        nvrhi::InputLayoutHandle GetInputLayout() { return m_InputLayout; }

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

        static Ref<GraphicsPipeline> Create(const std::string& name = "Graphics Pipeline");

        GraphicsPipelineParams &GetParams() { return m_Params; }

    private:
        std::string m_Name;
        nvrhi::GraphicsPipelineHandle m_Handle;
        std::vector<Ref<Shader>> m_Shaders;
        nvrhi::InputLayoutHandle m_InputLayout;
        std::vector<nvrhi::BindingLayoutHandle> m_BindingLayouts;

        GraphicsPipelineParams m_Params;

        bool m_NeedsToCompileShader = false;
    };
}

#endif
