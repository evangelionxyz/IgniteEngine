// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_RENDERER_HPP
#define WIDGET_RENDERER_HPP

#include "ignite/core/types.hpp"
#include "ignite/graphics/ui/widget.hpp"

#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace ignite
{
    class IWidgetItem;
    class RenderTarget;
    class Renderer2D;
    class ConstantBuffer;

    class WidgetRenderer
    {
    public:
        WidgetRenderer(uint32_t width, uint32_t height);
        ~WidgetRenderer();

        void SetMousePosition(uint32_t mouseX, uint32_t mouseY);

        void Update(float deltaTime);
        void Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb);
        void Resize(uint32_t width, uint32_t height);

        static Ref<WidgetRenderer> Create(uint32_t width, uint32_t height);
        
        Ref<Renderer2D> GetRenderer() { return m_Renderer2D; }

        const uint32_t &GetWidth() { return m_Width; }
        const uint32_t &GetHeight() { return m_Height; }

    private:
        void RenderWidgetItems(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb);

        uint32_t m_Width;
        uint32_t m_Height;

        uint32_t m_MouseX;
        uint32_t m_MouseY;

        Ref<Renderer2D> m_Renderer2D;
        Ref<ConstantBuffer> m_CameraBuffer;
        Widget *m_Widget = nullptr;

        glm::mat4 m_Projection;
    };
}

#endif
