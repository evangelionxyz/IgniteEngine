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

#include "renderer.hpp"

#include "ignite/core/application.hpp"

namespace ignite {

    GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineParams &params, GraphicsPipelineCreateInfo *createInfo)
        : m_Params(params), m_CreateInfo(std::move(createInfo))
    {
    }

    GraphicsPipeline& GraphicsPipeline::AddBindingLayout(const nvrhi::BindingLayoutHandle& layout)
    {
        m_BindingLayouts.emplace_back(layout);
        return *this;
    }

    GraphicsPipeline& GraphicsPipeline::AddShader(const std::string& filepath, nvrhi::ShaderType type, const std::string &entryPoint, bool recompile)
    {
        ShaderMake::ShaderType shaderType = GetShaderMakeShaderType(type);

        ShaderMake::ShaderContextDesc desc;
        desc.entryPoint = entryPoint;
        
        Ref<ShaderMake::ShaderContext> context = CreateRef<ShaderMake::ShaderContext>(filepath, shaderType, desc, recompile);
        m_ShaderContexts.push_back(std::move(context));

        m_NeedsToCompileShader = true;

        return *this;
    }

    GraphicsPipeline& GraphicsPipeline::AddShader(nvrhi::ShaderHandle& handle, nvrhi::ShaderType type)
    {
        m_Shaders[type] = handle;
        return *this;
    }

    void GraphicsPipeline::CreatePipeline(nvrhi::IFramebuffer *framebuffer)
    {
        if (m_Handle == nullptr)
        {
            // create graphics pipeline
            nvrhi::BlendState blendState;
            blendState.targets[0].blendEnable = m_Params.enableBlend;
            // blendState.targets[1].blendEnable = false;
            // blendState.targets[1].colorWriteMask = nvrhi::ColorMask::All;

            nvrhi::DepthStencilState depthStencilState;
            depthStencilState.depthWriteEnable = m_Params.depthWrite;
            depthStencilState.depthTestEnable = m_Params.depthTest;
            depthStencilState.depthFunc = m_Params.comparison;

            depthStencilState.stencilEnable = m_Params.enableDepthStencil;
            depthStencilState.frontFaceStencil = m_Params.frontFaceStencilDesc;
            depthStencilState.backFaceStencil = m_Params.backFaceStencilDesc;
            depthStencilState.stencilWriteMask = m_Params.stencilWriteMask;
            depthStencilState.stencilReadMask = m_Params.stencilReadMask;
            depthStencilState.stencilRefValue = m_Params.stencilRefValue;

            nvrhi::RasterState rasterState;

            rasterState.cullMode = m_Params.cullMode;
            rasterState.fillMode = m_Params.fillMode;
            rasterState.setFrontCounterClockwise(false);
            rasterState.setMultisampleEnable(false);

            nvrhi::RenderState renderState;
            renderState.setRasterState(rasterState);
            renderState.setDepthStencilState(depthStencilState);
            renderState.setBlendState(blendState);

            nvrhi::GraphicsPipelineDesc pipelineDesc;

            for (auto& shader : m_Shaders)
            {
                if (shader.first == nvrhi::ShaderType::Vertex)
                    pipelineDesc.setVertexShader(shader.second);
                else if (shader.first == nvrhi::ShaderType::Pixel)
                    pipelineDesc.setPixelShader(shader.second);
            }

            pipelineDesc.setInputLayout(m_InputLayout);
            pipelineDesc.setRenderState(renderState);
            pipelineDesc.primType = m_Params.primitiveType;

            for (auto &layout : m_BindingLayouts)
                pipelineDesc.addBindingLayout(layout);

            // create with the same framebuffer to be rendered
            nvrhi::IDevice* device = Application::GetGraphicsDevice();

            m_Handle = device->createGraphicsPipeline(pipelineDesc, framebuffer);
            LOG_ASSERT(m_Handle, "Failed to create graphics pipeline");
        }
    }

    void GraphicsPipeline::ResetHandle()
    {
        m_Handle = nullptr;
    }

    nvrhi::BindingLayoutHandle GraphicsPipeline::GetBindingLayout(uint32_t index)
    {
        if (index < m_BindingLayouts.size())
            return m_BindingLayouts[index];
        return nullptr;
    }

    void GraphicsPipeline::Build()
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        if (m_NeedsToCompileShader)
        {
            Renderer::GetShaderLibrary().GetContext()->CompileShader(m_ShaderContexts);

            for (auto& context : m_ShaderContexts)
            {
                nvrhi::ShaderType shaderType = GetNVRHIShaderType(context->GetType());
                m_Shaders[shaderType] = device->createShader(shaderType, context->blob.data.data(), context->blob.dataSize());

                LOG_ASSERT(m_Shaders[shaderType], "[Graphics Pipeline] Failed to create shader");
            }

            m_ShaderContexts.clear();
        }

        m_InputLayout = device->createInputLayout(m_CreateInfo->attributes, m_CreateInfo->attributeCount, nullptr);
        LOG_ASSERT(m_InputLayout, "[Graphics Pipeline] Failed to create input layout");
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create(const GraphicsPipelineParams &params, GraphicsPipelineCreateInfo *createInfo)
    {
        return CreateRef<GraphicsPipeline>(params, createInfo);
    }

}
