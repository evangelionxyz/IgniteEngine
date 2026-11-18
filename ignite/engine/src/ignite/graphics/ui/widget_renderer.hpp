// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WIDGET_RENDERER_HPP
#define IGN_WIDGET_RENDERER_HPP

#include "ignite/core/types.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/renderer/batch_data.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace ignite
{
    class IWidgetItem;
    class Scene;
    class Project;
    class RenderTarget;
    class Renderer2D;
    class ConstantBuffer;
    class Font;

    class WidgetRenderer
    {
    public:
        WidgetRenderer(uint32_t width, uint32_t height);
        ~WidgetRenderer();

        void Begin(nri::CommandBuffer *cmd);
        void Flush(RenderTarget *framebuffer);

        void DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f));
        void DrawRoundedQuad(const Rect &rect, float cornerRadius, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor = glm::vec2(1.0f));
        void DrawRoundedBorder(const Rect &rect, float cornerRadius, float borderWidth, const glm::vec4 &borderColor, const glm::vec2 &tilingFactor = glm::vec2(4.0f, 0.0f));
        void DrawString(const std::string &str, const Ref<Font> &font, const glm::vec4 &color, const glm::mat4 &transform, float kerning, float linespacing);

        void SetActiveWidget(const Ref<WidgetCanvas> &widget) { m_ActiveWidget = widget; }
        void SetMousePosition(uint32_t mouseX, uint32_t mouseY);
        void SetCanvasSize(uint32_t width, uint32_t height);
        void SetViewMatrix(const glm::mat4 &view) { m_View = view; }

        void Update(float deltaTime);
        void Render(nri::CommandBuffer *cmd, RenderTarget *fb);
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetOrInsertQuadTexture(const Ref<Texture> &texture);
        uint32_t GetOrInsertFontTexture(const Ref<Texture> &texture);

        static Ref<WidgetRenderer> Create(uint32_t width, uint32_t height);
        static void ClearCache();

        const uint32_t &GetWidth() { return m_Width; }
        const uint32_t &GetHeight() { return m_Height; }
        uint32_t GetCanvasWidth() const { return m_CanvasWidth > 0 ? m_CanvasWidth : m_Width; }
        uint32_t GetCanvasHeight() const { return m_CanvasHeight > 0 ? m_CanvasHeight : m_Height; }

    private:
        struct WidgetRenderLayer
        {
            Ref<WidgetCanvas> widget;
            bool blocksWidgetsBelow = false;
        };

        void BuildRenderLayers();
        void RenderWidgetItems();

        void InitQuadData();
        void InitTextData();

        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_CanvasWidth = 0;
        uint32_t m_CanvasHeight = 0;

        uint32_t m_MouseX;
        uint32_t m_MouseY;

        nri::CommandBuffer *m_Cmd = nullptr;
        Ref<ConstantBuffer> m_CameraBuffer;

        BatchRender<VertexWidgetQuad> m_QuadBatch;
        uint32_t *m_QuadIndicesBase = nullptr;
        uint32_t *m_QuadIndicesPtr = nullptr;
        BatchRender<VertexWidgetText> m_TextBatch;

        Ref<WidgetCanvas> m_ActiveWidget = nullptr;
        std::vector<WidgetRenderLayer> m_RenderLayers;

        glm::mat4 m_Projection;
        glm::mat4 m_View = glm::mat4(1.0f);
    };
}

#endif
