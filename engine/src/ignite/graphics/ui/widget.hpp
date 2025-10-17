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

#include "ignite/core/types.hpp"
#include "ignite/math/math.hpp"
#include "ignite/core/logger.hpp"

#include <string>
#include <functional>
#include <memory>
#include <glm/glm.hpp>

// In Game Widget
namespace ignite {

#define UI_COLOR_RED glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f }
#define UI_COLOR_BLUE glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f }
#define UI_COLOR_WHITE glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
#define UI_COLOR_GREEN glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_YELLOW glm::vec4{ 1.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_GRAY glm::vec4{ 0.5f, 0.5f, 0.5f, 1.0f }

    class Texture;

    enum class UIAlignment
    {
        TOP_LEFT,
        TOP_CENTER,
        TOP_RIGHT,
        CENTER_LEFT,
        CENTER,
        CENTER_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_CENTER,
        BOTTOM_RIGHT,
        COUNT
    };

    class UIWidget : public std::enable_shared_from_this<UIWidget>
    {
    public:
        UIWidget() = default;
        virtual ~UIWidget() = default;

        virtual void Update(float deltaTime, const glm::vec2& mousePos)
        {
        }

        virtual void SetPosition(const glm::vec2 &position) 
        {
            m_Rect.min = position;
        }

        virtual void SetSize(const glm::vec2 &size)
        {
            m_Rect.max = size;
        }

        virtual void SetAlignment(UIAlignment alignment)
        {
            m_Alignment = alignment;
        }

        virtual const glm::vec2 GetSize() const { return m_Rect.GetSize(); }
        virtual const Rect &GetRect() const { return m_Rect; }
        virtual const UIAlignment GetAlignment() const { return m_Alignment; }

        // Check if point is within widget bounds
        virtual bool Contains(const glm::vec2& point) const
        {
            return GetAlignedRect().Contains(point);
        }

        // Get position adjusted for alignment relative to canvas (viewport)
        virtual Rect GetAlignedRect() const;

        template<typename T>
        Ref<T> As()
        {
            return std::dynamic_pointer_cast<T>(shared_from_this());
        }

    protected:
        Rect m_Rect = Rect({0.0f, 0.0f}, {100.0f, 50.0f});
        UIAlignment m_Alignment = UIAlignment::TOP_LEFT;
    };

    class UIButton : public UIWidget
    {
    public:
        UIButton(const std::string& text = "Button")
            : m_Text(text)
        {
            m_NormalColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
            m_HoverColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            m_PressedColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
            m_TextColor = UI_COLOR_WHITE;
            m_BorderColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        }

        ~UIButton()
        {
        }

        void Update(float deltaTime, const glm::vec2& mousePos) override
        {
            bool wasHovered = m_IsHovered;
            m_IsHovered = Contains(mousePos);
            
            // Trigger hover events
            if (m_IsHovered && !wasHovered && m_OnHoverEnter)
            {
                m_OnHoverEnter();
            }
            else if (!m_IsHovered && wasHovered && m_OnHoverExit)
            {
                m_OnHoverExit();
            }
        }

        // Handle mouse click (call this from input system)
        void OnMouseClick(const glm::vec2& mousePos, bool isPressed)
        {
            if (Contains(mousePos))
            {
                if (isPressed)
                {
                    m_IsPressed = true;
                    if (m_OnPressed)
                        m_OnPressed();
                }
                else if (m_IsPressed)
                {
                    m_IsPressed = false;
                    if (m_OnClick)
                        m_OnClick();
                    if (m_OnReleased)
                        m_OnReleased();
                }
            }
            else if (!isPressed)
            {
                m_IsPressed = false;
            }
        }

        // Getters
        const std::string& GetText() const { return m_Text; }
        const Ref<Texture> &GetImage() const { return m_Image; }

        bool IsHovered() const { return m_IsHovered; }
        bool IsPressed() const { return m_IsPressed; }

        glm::vec4 GetCurrentColor() const
        {
            if (m_IsPressed) return m_PressedColor;
            if (m_IsHovered) return m_HoverColor;
            return m_NormalColor;
        }

        const glm::vec4& GetTextColor() const { return m_TextColor; }
        const glm::vec4& GetBorderColor() const { return m_BorderColor; }

        // Setters
        void SetText(const std::string& text) { m_Text = text; }
        void SetColors(const glm::vec4& normal, const glm::vec4& hover, const glm::vec4& pressed)
        {
            m_NormalColor = normal;
            m_HoverColor = hover;
            m_PressedColor = pressed;
        }
        void SetTextColor(const glm::vec4& color) { m_TextColor = color; }
        void SetBorderColor(const glm::vec4& color) { m_BorderColor = color; }
        void SetImage(const Ref<Texture> &image) { m_Image = image; }

        // Event callbacks
        void SetOnClick(std::function<void()> callback) { m_OnClick = callback; }
        void SetOnPressed(std::function<void()> callback) { m_OnPressed = callback; }
        void SetOnReleased(std::function<void()> callback) { m_OnReleased = callback; }
        void SetOnHoverEnter(std::function<void()> callback) { m_OnHoverEnter = callback; }
        void SetOnHoverExit(std::function<void()> callback) { m_OnHoverExit = callback; }

    private:
        std::string m_Text;
        bool m_IsHovered = false;
        bool m_IsPressed = false;

        Ref<Texture> m_Image;

        glm::vec4 m_NormalColor;
        glm::vec4 m_HoverColor;
        glm::vec4 m_PressedColor;
        glm::vec4 m_TextColor;
        glm::vec4 m_BorderColor;

        std::function<void()> m_OnClick;
        std::function<void()> m_OnPressed;
        std::function<void()> m_OnReleased;
        std::function<void()> m_OnHoverEnter;
        std::function<void()> m_OnHoverExit;
    };

    class UIText : public UIWidget
    {
    public:
        UIText(const std::string& text = "Text")
            : m_Text(text), m_Color(UI_COLOR_WHITE)
        {
        }

        const std::string& GetText() const { return m_Text; }
        const glm::vec4& GetColor() const { return m_Color; }

        void SetText(const std::string& text) { m_Text = text; }
        void SetColor(const glm::vec4& color) { m_Color = color; }

    private:
        std::string m_Text;
        glm::vec4 m_Color;
    };

    // Layout Grid for visualizing UI layout
    class UILayoutGrid
    {
    public:
        UILayoutGrid(uint32_t viewportWidth, uint32_t viewportHeight, uint32_t gridSize = 20)
            : m_ViewportWidth(viewportWidth), m_ViewportHeight(viewportHeight), m_GridSize(gridSize), m_Visible(true)
        {
            m_GridColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.3f);
            m_MajorGridColor = glm::vec4(0.7f, 0.7f, 0.7f, 0.5f);
            m_MajorGridInterval = 5; // Every 5th line is major
        }

        void SetViewportSize(uint32_t width, uint32_t height)
        {
            m_ViewportWidth = width;
            m_ViewportHeight = height;
        }

        void SetGridSize(uint32_t size) { m_GridSize = size; }
        void SetVisible(bool visible) { m_Visible = visible; }
        void SetGridColor(const glm::vec4& color) { m_GridColor = color; }
        void SetMajorGridColor(const glm::vec4& color) { m_MajorGridColor = color; }
        void SetMajorGridInterval(uint32_t interval) { m_MajorGridInterval = interval; }

        bool IsVisible() const { return m_Visible; }
        uint32_t GetGridSize() const { return m_GridSize; }
        const glm::vec4& GetGridColor() const { return m_GridColor; }
        const glm::vec4& GetMajorGridColor() const { return m_MajorGridColor; }
        uint32_t GetMajorGridInterval() const { return m_MajorGridInterval; }
        uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }

        // Get grid lines for rendering
        struct GridLine
        {
            glm::vec2 start;
            glm::vec2 end;
            bool isMajor;
        };

        std::vector<GridLine> GetGridLines() const
        {
            std::vector<GridLine> lines;
            if (!m_Visible)
                   return lines;

            // Vertical lines
            for (uint32_t x = 0; x <= m_ViewportWidth; x += m_GridSize)
            {
                bool isMajor = (x / m_GridSize) % m_MajorGridInterval == 0;
                lines.push_back({
                    {static_cast<float>(x), 0.0f},
                    {static_cast<float>(x), static_cast<float>(m_ViewportHeight)},
                    isMajor
                });
            }

            // Horizontal lines
            for (uint32_t y = 0; y <= m_ViewportHeight; y += m_GridSize)
            {
                bool isMajor = (y / m_GridSize) % m_MajorGridInterval == 0;
                lines.push_back({
                    {0.0f, static_cast<float>(y)},
                    {static_cast<float>(m_ViewportWidth), static_cast<float>(y)},
                    isMajor
                });
            }

            return lines;
        }

        // Snap position to grid
        glm::vec2 SnapToGrid(const glm::vec2& position) const
        {
            return glm::vec2(
                std::round(position.x / m_GridSize) * m_GridSize,
                std::round(position.y / m_GridSize) * m_GridSize
            );
        }

    private:
        uint32_t m_ViewportWidth;
        uint32_t m_ViewportHeight;
        uint32_t m_GridSize;
        uint32_t m_MajorGridInterval;
        bool m_Visible;
        glm::vec4 m_GridColor;
        glm::vec4 m_MajorGridColor;
    };
}
