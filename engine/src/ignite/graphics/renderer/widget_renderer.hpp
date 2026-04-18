// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_RENDERER_HPP
#define WIDGET_RENDERER_HPP

#include "ignite/core/types.hpp"
#include "ignite/graphics/ui/widget.hpp"

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

    class WidgetRenderer
    {
    public:
        WidgetRenderer(uint32_t width, uint32_t height);
        ~WidgetRenderer();

        void SetScene(Scene *scene) { m_Scene = scene; }
        void SetProject(Project *project) { m_Project = project; }
        void SetPreviewWidget(const Ref<WidgetCanvas> &widget) { m_PreviewWidget = widget; }
        void SetMousePosition(uint32_t mouseX, uint32_t mouseY);

        void Update(float deltaTime);
        void Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb);
        void Resize(uint32_t width, uint32_t height);

        static Ref<WidgetRenderer> Create(uint32_t width, uint32_t height);
        
        Ref<Renderer2D> GetRenderer() { return m_Renderer2D; }

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

        uint32_t m_Width;
        uint32_t m_Height;

        uint32_t m_MouseX;
        uint32_t m_MouseY;

        Ref<Renderer2D> m_Renderer2D;
        Ref<ConstantBuffer> m_CameraBuffer;
        Scene *m_Scene = nullptr;
        Project *m_Project = nullptr;
        Ref<WidgetCanvas> m_PreviewWidget = nullptr;
        std::vector<WidgetRenderLayer> m_RenderLayers;

        glm::mat4 m_Projection;
    };
}

#endif
