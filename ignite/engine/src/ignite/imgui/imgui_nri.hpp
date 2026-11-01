// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_IMGUI_NRI_HPP
#define IGN_IMGUI_NRI_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/texture.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <array>
#include <optional>
#include <cstdint>
#include <mutex>
#include <glm/glm.hpp>

#include <NRI.h>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
    #define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui.h>

namespace ignite
{
    class RenderTarget;

    struct ImGuiVertexData
    {
        glm::vec2 position;
        glm::vec2 texCoord;
        glm::vec4 color;
    };

    struct ImGuiPushConstants
    {
        std::array<float, 2> invDisplaySize;
        std::array<float, 2> displayPos;
    };

    class IGN_API ImGui_NRI
    {
    public:
        ImGui_NRI() = default;
        ~ImGui_NRI();

        bool Init(nri::Device *device);
        void Shutdown();
        bool UpdateFontTexture();

        bool Render(RenderTarget *renderTarget);
        bool Render(ImDrawData *drawData, RenderTarget *renderTarget);
        bool Render(nri::CommandBuffer *cmd, ImDrawData *drawData, RenderTarget *renderTarget, uint32_t slotIndex = 0);

        void BackBufferResizing();

        static void InvalidateTextureCache(void *textureHandle);
        static void InvalidateTextureCache(nri::Descriptor *textureSRV);
        static void ClearTextureCache();

        static ImGui_NRI *s_Instance;

        Ref<Texture> GetFontTexture() const { return m_FontTexture; }
        nri::Descriptor *GetFontSampler() const { return m_FontSampler; }
        void RemoveGraphicsPipeline(const FramebufferKey &key) { m_GraphicsPipelines.erase(key); }

    private:
        bool UpdateGeometry(ImDrawData *drawData, uint32_t slotIndex);
        Ref<GraphicsPipeline> GetPSO(RenderTarget *renderTarget);
        Ref<BindingSet> GetBindingSet(nri::Descriptor *textureSRV);

        nri::CommandBuffer *BeginCommandBuffer(uint32_t &slotIndex);
        void SubmitCommandBuffer(nri::CommandBuffer *cmd);

        nri::Device *m_Device = nullptr;

        struct CommandBufferSlot
        {
            nri::CommandAllocator *allocator = nullptr;
            nri::CommandBuffer *commandBuffer = nullptr;
            uint64_t fenceValue = 0;
        };

        std::array<CommandBufferSlot, 3> m_CommandSlots{};
        uint32_t m_CurrentSlotIndex = 0;
        nri::Fence *m_CommandFence = nullptr;
        uint64_t m_NextFenceValue = 1;

        Ref<Texture> m_FontTexture;
        nri::Descriptor *m_FontSampler = nullptr;

        struct GeometryBufferSlot
        {
            Ref<VertexBuffer> vertexBuffer;
            Ref<IndexBuffer> indexBuffer;
            std::vector<ImGuiVertexData> vertexBufferCPU;
            std::vector<ImDrawIdx> indexBufferCPU;
        };

        std::array<GeometryBufferSlot, 3> m_GeometryBufferSlots{};

        std::optional<BindingLayoutDesc> m_BindingLayoutDesc;
        nri::PipelineLayout *m_PipelineLayout = nullptr;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_GraphicsPipelines;
        std::mutex m_BindingsCacheMutex;
        std::unordered_map<nri::Descriptor *, Ref<BindingSet>> m_BindingsCache;

        bool m_IsShuttingDown = false;

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

    using ImGui_NVRHI = ImGui_NRI;
}

#endif
