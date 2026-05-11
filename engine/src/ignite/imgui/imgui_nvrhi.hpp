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

#pragma once

#include "ignite/core/types.hpp"
#include "ignite/graphics/shader.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <stdint.h>
#include <glm/glm.hpp>

#include <nvrhi/nvrhi.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace ignite
{
    class ShaderFactory;
    class GraphicsPipeline;

    struct ImGuiVertexData
    {
        glm::vec2 position;
        glm::vec2 texCoord;
        glm::vec4 color;
    };

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
        std::unordered_map<nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> graphicsPipelines;
        std::unordered_map<nvrhi::ITexture *, nvrhi::BindingSetHandle> bindingsCache;

        // Called from Texture::~Texture() to evict a destroyed GPU texture handle
        // from the binding cache before its pointer address can be reused.
        static void InvalidateTextureCache(nvrhi::ITexture *texture);
        static ImGui_NVRHI *s_Instance;

        std::vector<ImGuiVertexData> imguiVertexBuffer;
        std::vector<ImDrawIdx> imguiIndexBuffer;

        bool m_IsShuttingDown = false;

        bool Init(nvrhi::IDevice *device);
        void Shutdown();
        bool UpdateFontTexture();
        bool Render(nvrhi::IFramebuffer *framebuffer);
        bool Render(ImDrawData *drawData, nvrhi::IFramebuffer *framebuffer);
        void BackBufferResizing();

    private:
        bool ReallocateBuffer(nvrhi::BufferHandle &buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
        Ref<GraphicsPipeline> GetPSO(nvrhi::IFramebuffer *framebuffer);
        nvrhi::IBindingSet *GetBindingSet(nvrhi::ITexture *texture, nvrhi::BindingLayoutHandle bindingLayout);
        bool UpdateGeometry(nvrhi::ICommandList *commandList, ImDrawData *drawData);

#ifdef PLATFORM_WINDOWS
        static void RendererCreateWindow(ImGuiViewport *viewport);
        static void RendererDestroyWindow(ImGuiViewport *viewport);
        static void RendererSetWindowSize(ImGuiViewport *viewport, ImVec2 size);
        static void RendererRenderWindow(ImGuiViewport *viewport, void *);
        static void RendererSwapBuffers(ImGuiViewport *viewport, void *);
#endif

#ifdef IGNITE_WITH_VULKAN
        static void RendererCreateWindowVK(ImGuiViewport *viewport);
        static void RendererDestroyWindowVK(ImGuiViewport *viewport);
        static void RendererSetWindowSizeVK(ImGuiViewport *viewport, ImVec2 size);
        static void RendererRenderWindowVK(ImGuiViewport *viewport, void *);
        static void RendererSwapBuffersVK(ImGuiViewport *viewport, void *);
#endif

    };
}
