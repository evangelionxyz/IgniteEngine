// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_renderer.hpp"
#include "renderer_2d.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"

namespace ignite
{
    WidgetRenderer::WidgetRenderer(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height), m_MouseX(0), m_MouseY(0)
    {
        m_Renderer2D = Renderer2D::Create();
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);

        m_CameraBuffer = ConstantBuffer::Create(sizeof(CameraBufferData), false, 1, "[WidgetRenderer] Camera buffer");
    }

    WidgetRenderer::~WidgetRenderer()
    {
        m_CameraBuffer = nullptr;
    }

    void WidgetRenderer::SetMousePosition(uint32_t mouseX, uint32_t mouseY)
    {
        m_MouseX = mouseX;
        m_MouseY = mouseY;
    }

    void WidgetRenderer::Update(float deltaTime)
    {
        m_Widget->Update(deltaTime, { m_MouseX, m_MouseY });
    }

    void WidgetRenderer::Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb)
    {
        CameraBufferData cameraData = { m_Projection, glm::mat4(1.0f), {0.0f, 0.0f, 0.0f, 1.0f} };
        m_CameraBuffer->SetData(cmd, Buffer(&cameraData, sizeof(cameraData)));

        m_Renderer2D->Begin(cmd);
        
        RenderWidgetItems(cmd, fb);

        m_Renderer2D->Flush(fb, m_CameraBuffer);
        m_Renderer2D->End();
    }

    void WidgetRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
        m_Widget->SetViewportSize(width, height);
    }

    void WidgetRenderer::RenderWidgetItems(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb)
    {
        for (auto &[id, widget] : m_Widget->GetItems())
        {
            if (!widget)
                continue;

            // Render different widget types
            if (auto button = std::dynamic_pointer_cast<WidgetButton>(widget))
            {
                const Rect &rect = button->GetAlignedRect();
                const glm::vec4 &buttonColor = button->GetCurrentColor();
                const Ref<Texture> image = button->GetImage();

                m_Renderer2D->DrawQuad(rect, 0.0f, buttonColor, image, { 0.0f, 1.0f }, { 1.0f, 0.0f });
            }
            else if (auto text = std::dynamic_pointer_cast<WidgetText>(widget))
            {
            }
        }
    }

    Ref<WidgetRenderer> WidgetRenderer::Create(uint32_t width, uint32_t height)
    {
        return CreateRef<WidgetRenderer>(width, height);
    }
}