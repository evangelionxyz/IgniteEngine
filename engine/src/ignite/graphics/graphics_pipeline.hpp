/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
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

#pragma once

#include "ignite/core/types.hpp"
#include "shader.hpp"

#include <nvrhi/nvrhi.h>
#include <vector>
#include <unordered_map>

namespace ignite {

    struct GraphicsPipelineCreateInfo
    {
        nvrhi::VertexAttributeDesc *attributes;
        uint32_t attributeCount = 0;
    };

    struct GraphicsPipelineParams
    {
        nvrhi::RasterCullMode cullMode = nvrhi::RasterCullMode::Front;
        nvrhi::ComparisonFunc comparison = nvrhi::ComparisonFunc::LessOrEqual;
        nvrhi::PrimitiveType primitiveType = nvrhi::PrimitiveType::TriangleList;
        nvrhi::RasterFillMode fillMode = nvrhi::RasterFillMode::Solid;

        nvrhi::DepthStencilState::StencilOpDesc frontFaceStencilDesc;
        nvrhi::DepthStencilState::StencilOpDesc backFaceStencilDesc;

        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;
        uint8_t stencilRefValue = 0;

        bool enableBlend = true;
        bool enableDepthStencil = false;
        bool depthWrite = false;
        bool depthTest = false;
    };

    class GraphicsPipeline
    {
    public:
        GraphicsPipeline() = default;
        GraphicsPipeline(const GraphicsPipelineParams &params, GraphicsPipelineCreateInfo *createInfo);

        GraphicsPipeline &AddBindingLayout(const nvrhi::BindingLayoutHandle &layout);
        GraphicsPipeline& AddShader(const std::string& filepath, nvrhi::ShaderType type, const std::string &entryPoint = "main", bool recompile = false);
        GraphicsPipeline& AddShader(nvrhi::ShaderHandle& handle, nvrhi::ShaderType type);
        void Build();
        void CreatePipeline(nvrhi::IFramebuffer *framebuffer);
        void ResetHandle();

        nvrhi::BindingLayoutHandle GetBindingLayout(uint32_t index);

        nvrhi::GraphicsPipelineHandle GetHandle() { return m_Handle; }
        nvrhi::InputLayoutHandle GetInputLayout() { return m_InputLayout; }

        nvrhi::ShaderHandle GetShader(nvrhi::ShaderType type)
        {
            if (m_Shaders.contains(type))
                return m_Shaders[type];

            return nullptr;
        }

        static Ref<GraphicsPipeline> Create(const GraphicsPipelineParams &params, GraphicsPipelineCreateInfo *createInfo);

        GraphicsPipelineParams &GetParams() { return m_Params; }

    private:
        nvrhi::GraphicsPipelineHandle m_Handle;

        std::unordered_map<nvrhi::ShaderType, nvrhi::ShaderHandle> m_Shaders;
        std::vector<Ref<ShaderMake::ShaderContext>> m_ShaderContexts;

        nvrhi::InputLayoutHandle m_InputLayout;
        std::vector<nvrhi::BindingLayoutHandle> m_BindingLayouts;

        GraphicsPipelineParams m_Params;
        GraphicsPipelineCreateInfo *m_CreateInfo;

        bool m_NeedsToCompileShader = false;
    };
}
