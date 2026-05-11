/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef GRAPHICS_PIPELINE_HPP
#define GRAPHICS_PIPELINE_HPP

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
        GraphicsPipeline() = default;

        GraphicsPipeline &AddBindingLayout(const nvrhi::BindingLayoutHandle &layout);
        GraphicsPipeline &SetShaders(const std::vector<Ref<Shader>> &shaders, bool recompile = false);
        void Build(nvrhi::IFramebuffer *framebuffer, const GraphicsPipelineParams &params);

        nvrhi::BindingLayoutHandle GetBindingLayout(uint32_t index);

        nvrhi::GraphicsPipelineHandle GetHandle() { return m_Handle; }
        nvrhi::InputLayoutHandle GetInputLayout() { return m_InputLayout; }

        Ref<Shader> GetShader(ShaderType type)
        {
            auto it = std::find_if(m_Shaders.begin(), m_Shaders.end(), [type](Ref<Shader> shader)
            {
                return shader->GetType() == type;
            });

            if (it != m_Shaders.end())
            {
                return *it;
            }

            return nullptr;
        }

        static Ref<GraphicsPipeline> Create();

        GraphicsPipelineParams &GetParams() { return m_Params; }

    private:
        nvrhi::GraphicsPipelineHandle m_Handle;
        std::vector<Ref<Shader>> m_Shaders;
        nvrhi::InputLayoutHandle m_InputLayout;
        std::vector<nvrhi::BindingLayoutHandle> m_BindingLayouts;

        GraphicsPipelineParams m_Params;

        bool m_NeedsToCompileShader = false;
    };
}

#endif
