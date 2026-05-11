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
        nvrhi::IDevice* device = DeviceManager::GetInstance()->GetDevice();

        LOG_ASSERT(m_Handle == nullptr, "[GraphicsPipeline] Should not re-create pipeline");
        
        m_Params = params;

        // create graphics pipeline
        nvrhi::RenderState renderState;
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
        renderState.rasterState.multisampleEnable = false;

        nvrhi::GraphicsPipelineDesc pipelineDesc;

        for (const Ref<Shader> &shader : m_Shaders)
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
        m_Handle = device->createGraphicsPipeline(pipelineDesc, framebuffer->getFramebufferInfo());
        LOG_ASSERT(m_Handle, "Failed to create graphics pipeline");
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create()
    {
        return CreateRef<GraphicsPipeline>();
    }

}
