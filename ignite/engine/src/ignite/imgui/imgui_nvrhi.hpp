// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_IMGUI_NVRHI_HPP
#define IGN_IMGUI_NVRHI_HPP

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

#endif
