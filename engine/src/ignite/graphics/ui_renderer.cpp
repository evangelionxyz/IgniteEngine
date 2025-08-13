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

#include "ui_renderer.hpp"
#include "renderer_2d.hpp"

namespace ignite
{
    UIRenderer::UIRenderer(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height)
    {
        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);
    }

    UIRenderer::~UIRenderer()
    {

    }

    void UIRenderer::Update(float deltaTime)
    {
        if (m_UIManager)
        {
            m_UIManager->Update(deltaTime);
        }
    }

    void UIRenderer::Render(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        // 2D Pass
        CameraConstants cc{ m_Projection, { 0.0f, 0.0f, 0.0f, 1.0f } };

        // Begin 2D rendering
        Renderer2D::Begin(cmd, framebuffer, cc);

        // Render layout grid first (background)
        RenderLayoutGrid(cmd, framebuffer);

        // Render UI widgets
        RenderWidgets(cmd, framebuffer);

        // Flush and end 2D rendering
        Renderer2D::Flush();
        Renderer2D::End();
    }

    void UIRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;

        m_Projection = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f);

        if (m_UIManager)
        {
            m_UIManager->SetViewportSize(width, height);
        }
    }

    void UIRenderer::RenderLayoutGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_UIManager) return;

        const UILayoutGrid& grid = m_UIManager->GetLayoutGrid();
        if (!grid.IsVisible()) return;

        auto gridLines = grid.GetGridLines();
        
        for (const auto& line : gridLines)
        {
            glm::vec4 color = line.isMajor ? grid.GetMajorGridColor() : grid.GetGridColor();
            
            // Grid lines are already in the correct coordinate system
            // Just convert to screen space for rendering (flip Y)
            glm::vec3 start = { line.start.x, static_cast<float>(m_Height) - line.start.y, 0.0f };
            glm::vec3 end = { line.end.x, static_cast<float>(m_Height) - line.end.y, 0.0f };
            
            Renderer2D::DrawLine(start, end, color);
        }
    }

    void UIRenderer::RenderWidgets(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_UIManager) return;

        const auto& widgets = m_UIManager->GetWidgets();
        
        for (const auto& widget : widgets)
        {
            if (!widget) continue;

            // Render different widget types
            if (auto button = std::dynamic_pointer_cast<UIButton>(widget))
            {
                RenderButton(cmd, framebuffer, button);
            }
            else if (auto text = std::dynamic_pointer_cast<UIText>(widget))
            {
                RenderText(cmd, framebuffer, text);
            }
        }
    }

    void UIRenderer::RenderButton(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<UIButton> button)
    {
        glm::vec2 position = button->GetAlignedPosition();
        glm::vec2 size = button->GetSize();
        
        // Convert UI space to screen space (flip Y coordinate)
        glm::vec3 screenPos = { position.x, static_cast<float>(m_Height) - position.y - size.y, 0.0f };
        
        // Create transform matrix for the button
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), screenPos);
        transform = glm::scale(transform, { size.x, size.y, 1.0f });
        
        // Render button background
        glm::vec4 buttonColor = button->GetCurrentColor();
        Renderer2D::DrawQuad(transform, buttonColor);
        
        // Render button border
        glm::vec4 borderColor = button->GetBorderColor();
        Renderer2D::DrawRect(transform, borderColor);
        
        // TODO: Add text rendering when text system is available
        // For now, we just render the button rectangle
    }

    void UIRenderer::RenderText(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<UIText> text)
    {
        // TODO: Implement text rendering when text/font system is available
        // For now, just render a small quad as placeholder
        glm::vec2 position = text->GetAlignedPosition();
        glm::vec2 size = text->GetSize();
        
        // Convert UI space to screen space (flip Y coordinate)
        glm::vec3 screenPos = { position.x, static_cast<float>(m_Height) - position.y - size.y, 0.0f };
        
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), screenPos);
        transform = glm::scale(transform, { size.x, size.y, 1.0f });
        
        Renderer2D::DrawQuad(transform, text->GetColor());
    }

    Ref<UIRenderer> UIRenderer::Create(uint32_t width, uint32_t height)
    {
        return CreateRef<UIRenderer>(width, height);
    }

}