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

#pragma once

#include "widget.hpp"
#include "ignite/core/types.hpp"
#include <vector>
#include <memory>

namespace ignite
{
    // Helper class to calculate mouse position relative to viewport from ImGui image
    class UIMouseHelper
    {
    public:
        static glm::vec2 GetViewportMousePosition(const glm::vec2& screenMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize)
        {
            // Calculate mouse position relative to viewport
            return screenMousePos - viewportPos;
        }

        // Convert from screen space to UI space (flip Y coordinate for UI)
        static glm::vec2 ScreenToUISpace(const glm::vec2& screenPos, float viewportHeight)
        {
            return glm::vec2(screenPos.x, viewportHeight - screenPos.y);
        }
    };

    class UIManager
    {
    public:
        UIManager();
        ~UIManager();

        // Widget management
        Ref<UIButton> CreateButton(const std::string& text, const glm::vec2& position, const glm::vec2& size = glm::vec2(100.0f, 50.0f));
        Ref<UIText> CreateText(const std::string& text, const glm::vec2& position);
        void AddWidget(Ref<UIWidget> widget);
        void RemoveWidget(Ref<UIWidget> widget);
        void ClearWidgets();

        // Layout grid
        void SetLayoutGridVisible(bool visible) { m_LayoutGrid.SetVisible(visible); }
        void SetLayoutGridSize(uint32_t size) { m_LayoutGrid.SetGridSize(size); }
        UILayoutGrid& GetLayoutGrid() { return m_LayoutGrid; }

        // Update and input handling
        void Update(float deltaTime);
        void SetViewportSize(uint32_t width, uint32_t height);
        void SetMousePosition(const glm::vec2& screenMousePos, const glm::vec2& viewportPos, const glm::vec2& viewportSize);
        void HandleMouseClick(bool isPressed); // Call when mouse button is pressed/released

        // Getters
        const std::vector<Ref<UIWidget>>& GetWidgets() const { return m_Widgets; }
        const glm::vec2& GetMousePosition() const { return m_MousePosition; }
        uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }

        // Static instance for global access
        static UIManager& GetInstance();

    private:
        std::vector<Ref<UIWidget>> m_Widgets;
        UILayoutGrid m_LayoutGrid;
        
        glm::vec2 m_MousePosition = glm::vec2(0.0f);
        uint32_t m_ViewportWidth = 1280;
        uint32_t m_ViewportHeight = 720;
        
        bool m_MousePressed = false;
        bool m_LastMousePressed = false;
    };
}