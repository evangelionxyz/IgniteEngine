// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_RENDERER_HPP
#define WIDGET_RENDERER_HPP

#include "ignite/core/types.hpp"
#include "ignite/graphics/renderer/batch_data.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include <nvrhi/nvrhi.h>
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

        void Begin(nvrhi::ICommandList *cmd);
        void Flush(nvrhi::IFramebuffer *framebuffer);

        void DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1);
        void DrawRoundedQuad(const Rect &rect, float cornerRadius, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1);
        void DrawRoundedBorder(const Rect &rect, float cornerRadius, float borderWidth, const glm::vec4 &borderColor);
        void DrawString(const std::string &str, const Ref<Font> &font, const glm::vec4 &color, const glm::mat4 &transform, float kerning, float linespacing);

        void SetProject(Project *project) { m_Project = project; }
        void SetActiveWidget(const Ref<WidgetCanvas> &widget) { m_ActiveWidget = widget; }
        void SetMousePosition(uint32_t mouseX, uint32_t mouseY);

        void Update(float deltaTime);
        void Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb);
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetOrInsertQuadTexture(const Ref<Texture> &texture);
        uint32_t GetOrInsertFontTexture(const Ref<Texture> &texture);

        static Ref<WidgetRenderer> Create(uint32_t width, uint32_t height);

        const uint32_t &GetWidth() { return m_Width; }
        const uint32_t &GetHeight() { return m_Height; }

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

        uint32_t m_MouseX;
        uint32_t m_MouseY;

        nvrhi::ICommandList *m_Cmd;
        Ref<ConstantBuffer> m_CameraBuffer;

        BatchRender<VertexWidgetQuad> m_QuadBatch;
        uint32_t *m_QuadIndicesBase = nullptr;
        uint32_t *m_QuadIndicesPtr = nullptr;
        BatchRender<VertexWidgetText> m_TextBatch;

        Project *m_Project = nullptr;
        Ref<WidgetCanvas> m_ActiveWidget = nullptr;
        std::vector<WidgetRenderLayer> m_RenderLayers;

        glm::mat4 m_Projection;
    };
}

#endif
