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

#include "ui_manager.hpp"
#include <algorithm>

namespace ignite
{
    UIManager::UIManager()
        : m_LayoutGrid(m_ViewportWidth, m_ViewportHeight)
    {
    }

    UIManager::~UIManager()
    {
        ClearWidgets();
    }

    Ref<UIButton> UIManager::CreateButton(const std::string& text, const glm::vec2& position, const glm::vec2& size)
    {
        auto button = CreateRef<UIButton>(text);
        button->SetPosition(position);
        button->SetSize(size);
        AddWidget(button);
        return button;
    }

    Ref<UIText> UIManager::CreateText(const std::string& text, const glm::vec2& position)
    {
        auto textWidget = CreateRef<UIText>(text);
        textWidget->SetPosition(position);
        AddWidget(textWidget);
        return textWidget;
    }

    void UIManager::AddWidget(Ref<UIWidget> widget)
    {
        if (widget)
        {
            m_Widgets.push_back(widget);
        }
    }

    void UIManager::RemoveWidget(Ref<UIWidget> widget)
    {
        auto it = std::find(m_Widgets.begin(), m_Widgets.end(), widget);
        if (it != m_Widgets.end())
        {
            m_Widgets.erase(it);
        }
    }

    void UIManager::ClearWidgets()
    {
        m_Widgets.clear();
    }

    void UIManager::Update(float deltaTime)
    {
        // Update all widgets
        for (auto& widget : m_Widgets)
        {
            if (widget)
            {
                widget->Update(deltaTime, m_MousePosition);
            }
        }

        // Handle mouse clicks for buttons
        bool mouseClicked = m_MousePressed && !m_LastMousePressed; // Mouse just pressed
        bool mouseReleased = !m_MousePressed && m_LastMousePressed; // Mouse just released

        if (mouseClicked || mouseReleased)
        {
            for (auto& widget : m_Widgets)
            {
                if (auto button = std::dynamic_pointer_cast<UIButton>(widget))
                {
                    button->OnMouseClick(m_MousePosition, mouseClicked);
                }
            }
        }

        m_LastMousePressed = m_MousePressed;
    }

    void UIManager::SetViewportSize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        m_LayoutGrid.SetViewportSize(width, height);
    }

    void UIManager::SetMousePosition(const glm::vec2& screenMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize)
    {
        // Calculate mouse position relative to viewport
        glm::vec2 localMouse = UIMouseHelper::GetViewportMousePosition(screenMousePos, viewportPos, viewportSize);
        m_MousePosition = localMouse;
    }

    void UIManager::HandleMouseClick(bool isPressed)
    {
        m_MousePressed = isPressed;
    }

    UIManager& UIManager::GetInstance()
    {
        static UIManager instance;
        return instance;
    }
}