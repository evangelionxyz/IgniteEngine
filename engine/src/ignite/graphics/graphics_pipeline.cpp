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

#include "graphics_pipeline.hpp"
#include "ignite/core/logger.hpp"
#include "shader.hpp"

#include "renderer.hpp"

#include "ignite/core/application.hpp"

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
        m_NeedsToCompileShader = true;
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
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        LOG_ASSERT(m_Handle == nullptr, "[GraphicsPipeline] Should not re-create pipeline");
        
        m_Params = params;

        // create graphics pipeline
        nvrhi::BlendState blendState;
        blendState.targets[0].blendEnable = m_Params.enableBlend;
        blendState.targets[0].srcBlend = m_Params.srcBlend;
        blendState.targets[0].destBlend = m_Params.destBlend;
        blendState.targets[0].srcBlendAlpha = m_Params.srcBlendAlpha;
        blendState.targets[0].destBlendAlpha = m_Params.destBlendAlpha;

        nvrhi::DepthStencilState depthStencilState;
        depthStencilState.depthWriteEnable = m_Params.enableDepthWrite;
        depthStencilState.depthTestEnable = m_Params.enableDepthTest;
        depthStencilState.depthFunc = m_Params.depthFunc;

        depthStencilState.stencilEnable = m_Params.enableDepthStencil;
        depthStencilState.frontFaceStencil = m_Params.frontFaceStencilDesc;
        depthStencilState.backFaceStencil = m_Params.backFaceStencilDesc;
        depthStencilState.stencilWriteMask = m_Params.stencilWriteMask;
        depthStencilState.stencilReadMask = m_Params.stencilReadMask;
        depthStencilState.stencilRefValue = m_Params.stencilRefValue;

        nvrhi::RasterState rasterState;
        rasterState.cullMode = m_Params.cullMode;
        rasterState.fillMode = m_Params.fillMode;
        rasterState.scissorEnable = m_Params.enableScissor;
        rasterState.depthClipEnable = m_Params.enableDepthClip;
        rasterState.frontCounterClockwise = false;
        rasterState.multisampleEnable = false;

        nvrhi::RenderState renderState;
        renderState.rasterState = rasterState;
        renderState.depthStencilState =depthStencilState;
        renderState.blendState = blendState;

        nvrhi::GraphicsPipelineDesc pipelineDesc;

        for (Ref<Shader> shader : m_Shaders)
        {
            if (shader->GetType() == ShaderType::Vertex)
            {
                pipelineDesc.setVertexShader(shader->GetHandle());

				const auto &vertexAttributes = shader->GetVertexAttributes();
                m_InputLayout = device->createInputLayout(vertexAttributes.data(), static_cast<uint32_t>(vertexAttributes.size()), nullptr);
                LOG_ASSERT(m_InputLayout, "[Graphics Pipeline] Failed to create input layout");
            }
            else if (shader->GetType() == ShaderType::Pixel)
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
        m_Handle = device->createGraphicsPipeline(pipelineDesc, framebuffer);
        LOG_ASSERT(m_Handle, "Failed to create graphics pipeline");
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create()
    {
        return CreateRef<GraphicsPipeline>();
    }

}
