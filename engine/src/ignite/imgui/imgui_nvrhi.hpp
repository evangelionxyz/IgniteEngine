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
#include "ignite/graphics/shader.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <stdint.h>

#include <nvrhi/nvrhi.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace ignite
{
    class ShaderFactory;

    struct ImGui_NVRHI
    {
        nvrhi::DeviceHandle m_Device;
        nvrhi::CommandListHandle commandList;
        nvrhi::InputLayoutHandle attributeLayout;

        nvrhi::TextureHandle fontTexture;
        nvrhi::SamplerHandle fontSampler;

        nvrhi::BufferHandle vertexBuffer;
        nvrhi::BufferHandle indexBuffer;

        nvrhi::BindingLayoutHandle bindingLayout;
        nvrhi::GraphicsPipelineDesc graphicsPipelineDesc;

        nvrhi::GraphicsPipelineHandle graphicsPipeline;
        std::unordered_map<nvrhi::ITexture *, nvrhi::BindingSetHandle> bindingsCache;

        std::vector<ImDrawVert> imguiVertexBuffer;
        std::vector<ImDrawIdx> imguiIndexBuffer;

        bool Init(nvrhi::IDevice *device);
        void Shutdown();
        bool UpdateFontTexture();
        bool Render(nvrhi::IFramebuffer *framebuffer);
        void BackBufferResizing();

    private:
        bool ReallocateBuffer(nvrhi::BufferHandle &buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
        nvrhi::IGraphicsPipeline *GetPSO(nvrhi::IFramebuffer *framebuffer);
        nvrhi::IBindingSet *GetBindingSet(nvrhi::ITexture *texture);
        bool UpdateGeometry(nvrhi::ICommandList *commandList);
    };
}
