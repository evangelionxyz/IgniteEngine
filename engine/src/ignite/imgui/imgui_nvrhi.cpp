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

#include "imgui_nvrhi.hpp"
#include "ignite/core/logger.hpp"

#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"

// ImGui color channel shifts for ImU32 RGBA packed format
#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24

namespace ignite
{
    bool ImGui_NVRHI::UpdateFontTexture()
    {
        ImGuiIO &io = ImGui::GetIO();

        // If the font texture exists and is bound to ImGui, we're done.
        // Note: ImGui_Renderer will reset io.Fonts->TexID when new fonts are added.
        if (fontTexture && io.Fonts->TexID)
            return true;

        unsigned char *pixels;
        i32 width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels)
            return false;

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.debugName = "ImGui font texture";

        fontTexture = m_Device->createTexture(textureDesc);
        LOG_ASSERT(fontTexture, "Failed to create imgui font texture");

        commandList->open();

        commandList->beginTrackingTextureState(fontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
        commandList->writeTexture(fontTexture, 0, 0, pixels, width * 4);
        commandList->setPermanentTextureState(fontTexture, nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();

        commandList->close();
        m_Device->executeCommandList(commandList);

        io.Fonts->TexID = (ImTextureID)fontTexture.Get();

        return true;
    }

    bool ImGui_NVRHI::Init(nvrhi::IDevice *device)
    {
        m_Device = device;
        commandList = device->createCommandList();

        const auto desc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
            .setAllFilters(true);

        fontSampler = device->createSampler(desc);

        LOG_ASSERT(fontSampler, "Failed to create ImGui font sampler");
        if (!fontSampler)
            return false;

        return true;
    }

    bool ImGui_NVRHI::Render(nvrhi::IFramebuffer *framebuffer)
    {
        ImDrawData *drawData = ImGui::GetDrawData();
        const ImGuiIO &io = ImGui::GetIO();

        commandList->open();
        commandList->beginMarker("ImGui");

        if (!UpdateGeometry(commandList))
        {
            commandList->close();
            return false;
        }

        drawData->ScaleClipRects(io.DisplayFramebufferScale);

        f32 invDisplaySize[2] = { 1.0f / io.DisplaySize.x, 1.0f / io.DisplaySize.y };

        // setup graphics state
        nvrhi::GraphicsState drawState;
        drawState.framebuffer = framebuffer;
        LOG_ASSERT(drawState.framebuffer, "Invalid framebuffer");

        Ref<GraphicsPipeline> pipeline = GetPSO(drawState.framebuffer);
        drawState.pipeline = pipeline->GetHandle();

        drawState.viewport.viewports.push_back(
            nvrhi::Viewport(
                io.DisplaySize.x * io.DisplayFramebufferScale.x,
                io.DisplaySize.y * io.DisplayFramebufferScale.y
        ));

        drawState.viewport.scissorRects.resize(1);

        drawState.vertexBuffers = { { vertexBuffer, 0, 0 } };
        drawState.indexBuffer.buffer = indexBuffer;
        drawState.indexBuffer.format = sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT;
        drawState.indexBuffer.offset = 0;

        // render command list
        i32 vtxOffset = 0;
        i32 idxOffset = 0;
        for (i32 n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList *cmdList = drawData->CmdLists[n];

            for (i32 i = 0; i < cmdList->CmdBuffer.Size; ++i)
            {
                const ImDrawCmd *pCmd = &cmdList->CmdBuffer[i];

                if (pCmd->UserCallback)
                {
                    pCmd->UserCallback(cmdList, pCmd);
                }
                else
                {
                    drawState.bindings = { GetBindingSet((nvrhi::ITexture *)pCmd->TextureId, pipeline->GetBindingLayout(0)) };
                    LOG_ASSERT(drawState.bindings[0], "Invalid draw state binding");

                    drawState.viewport.scissorRects[0] = nvrhi::Rect(
                        int(pCmd->ClipRect.x),
                        int(pCmd->ClipRect.z),
                        int(pCmd->ClipRect.y),
                        int(pCmd->ClipRect.w)
                    );

                    nvrhi::DrawArguments drawArguments;
                    drawArguments.vertexCount = pCmd->ElemCount;
                    drawArguments.startVertexLocation = vtxOffset;
                    drawArguments.startIndexLocation = idxOffset;

                    commandList->setGraphicsState(drawState);
                    commandList->setPushConstants(invDisplaySize, sizeof(invDisplaySize));
                    commandList->drawIndexed(drawArguments);
                }
                idxOffset += pCmd->ElemCount;
            }
            vtxOffset += cmdList->VtxBuffer.Size;
        }

        commandList->endMarker();
        commandList->close();
        m_Device->executeCommandList(commandList);

        return true;
    }

    void ImGui_NVRHI::BackBufferResizing()
    {
        graphicsPipeline = nullptr;
    }

    bool ImGui_NVRHI::ReallocateBuffer(nvrhi::BufferHandle &buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
    {
        if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
        {
            nvrhi::BufferDesc desc;
            desc.byteSize = static_cast<u32>(reallocateSize);
            desc.debugName = indexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
            desc.canHaveUAVs = false;
            desc.isVertexBuffer = !isIndexBuffer;
            desc.isIndexBuffer = isIndexBuffer;
            desc.isDrawIndirectArgs = false;
            desc.isVolatile = false;
            desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
            desc.keepInitialState = true;

            buffer = m_Device->createBuffer(desc);

            if (!buffer)
                return false;
        }

        return true;
    }

    Ref<GraphicsPipeline> ImGui_NVRHI::GetPSO(nvrhi::IFramebuffer *framebuffer)
    {
        if (graphicsPipeline)
        {
            return graphicsPipeline;
        }

        auto vertexShader = Shader::Create("resources/shaders/imgui.vertex.hlsl", ShaderType::Vertex);
        auto pixelShader = Shader::Create("resources/shaders/imgui.pixel.hlsl", ShaderType::Pixel);

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings =
        {
            nvrhi::BindingLayoutItem::PushConstants(0, sizeof(f32) * 2),
            nvrhi::BindingLayoutItem::Texture_SRV(0),
            nvrhi::BindingLayoutItem::Sampler(0)
        };

        auto bindingLayout = m_Device->createBindingLayout(layoutDesc);

        GraphicsPipelineParams params;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        
        params.enableBlend = true; // Explicitly enable blending for ImGui
        params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
        params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        params.srcBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        params.destBlendAlpha = nvrhi::BlendFactor::Zero;

        params.enableDepthClip = true;
        params.enableDepthClip = true;

        params.enableDepthTest = false;
        params.enableDepthWrite = true;
        params.enableDepthStencil = false;

        graphicsPipeline = GraphicsPipeline::Create();
        graphicsPipeline->SetShaders({ vertexShader, pixelShader }).AddBindingLayout(bindingLayout).Build(framebuffer, params);
        return graphicsPipeline;
    }

    nvrhi::IBindingSet *ImGui_NVRHI::GetBindingSet(nvrhi::ITexture *texture, nvrhi::BindingLayoutHandle bindingLayout)
    {
        auto iter = bindingsCache.find(texture);
        if (iter != bindingsCache.end())
            return iter->second;

        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
            nvrhi::BindingSetItem::Texture_SRV(0, texture),
            nvrhi::BindingSetItem::Sampler(0, fontSampler)
        };

        nvrhi::BindingSetHandle binding;
        binding = m_Device->createBindingSet(desc, bindingLayout);
        LOG_ASSERT(binding, "Failed to create ImGui binding set");

        bindingsCache[texture] = binding;
        return binding;
    }

    bool ImGui_NVRHI::UpdateGeometry(nvrhi::ICommandList *commandList)
    {
        ImDrawData *drawData = ImGui::GetDrawData();

        // Calculate size needed for expanded vertices
        size_t expandedVertexSize = drawData->TotalVtxCount * sizeof(ImGuiVertexData);
        
        if (!ReallocateBuffer(vertexBuffer, expandedVertexSize,
            (drawData->TotalVtxCount + 5000) * sizeof(ImGuiVertexData),
            false))
        {
            return false;
        }

        if (!ReallocateBuffer(indexBuffer, drawData->TotalIdxCount * sizeof(ImDrawIdx),
            (drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
            true))
        {
            return false;
        }

        // Resize buffers to match expanded vertex format
        imguiVertexBuffer.resize(vertexBuffer->getDesc().byteSize / sizeof(ImGuiVertexData));
        imguiIndexBuffer.resize(indexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

        ImGuiVertexData *vtxDst = imguiVertexBuffer.data();
        ImDrawIdx *idxDst = imguiIndexBuffer.data();

        for (i32 n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList *cmdList = drawData->CmdLists[n];
            
            // Convert ImDrawVert to ImGuiVertexData (expand ImU32 color to float4)
            for (i32 i = 0; i < cmdList->VtxBuffer.Size; ++i)
            {
                const ImDrawVert& src = cmdList->VtxBuffer[i];
                ImGuiVertexData& dst = vtxDst[i];
                
                dst.position = glm::vec2(src.pos.x, src.pos.y);
                dst.texCoord = glm::vec2(src.uv.x, src.uv.y);
                
                // Convert packed RGBA ImU32 to float4
                dst.color.r = ((src.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                dst.color.g = ((src.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                dst.color.b = ((src.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                dst.color.a = ((src.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            }
            
            std::memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        commandList->writeBuffer(vertexBuffer, imguiVertexBuffer.data(), vertexBuffer->getDesc().byteSize);
        commandList->writeBuffer(indexBuffer, imguiIndexBuffer.data(), indexBuffer->getDesc().byteSize);

        return true;
    }

    void ImGui_NVRHI::Shutdown()
    {
        fontTexture = nullptr;
        fontSampler = nullptr;

        vertexBuffer = nullptr;
        indexBuffer = nullptr;

        commandList = nullptr;
    }
}
