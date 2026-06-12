// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "widget_button.hpp"
#include "widget_label.hpp"

namespace ignite
{
    WidgetButton::WidgetButton(const std::string &text, WidgetID wID)
        : label(CreateScope<WidgetLabel>(text, (WidgetID)-1)), IWidgetItem(wID)
    {
        style.color = UI_COLOR_DARK_GRAY;
        style.hoverColor = UI_COLOR_GRAY;
        style.pressedColor = UI_COLOR_DARK_GRAY;
        style.borderColor = UI_COLOR_GRAY;
        style.cornerRadius = 0.0f;
        style.borderWidth = 1.0f;
    }

    WidgetButton::~WidgetButton()
    {
        image = nullptr;
    }

    const glm::vec4 &WidgetButton::GetCurrentColor() const
    {
        if (pressed)
            return style.pressedColor;
        if (hovered)
            return style.hoverColor;

        return style.color;
    }

    void WidgetButton::OnMouseClick(const glm::uvec2 &mousePos, bool isPressed)
    {
        const bool contains = HitTest(static_cast<int>(mousePos.x), static_cast<int>(mousePos.y));

        if (contains != hovered)
        {
            if (contains)
            {
                if (IWidgetItem::m_OnHoverEnter)
                {
                    IWidgetItem::m_OnHoverEnter();
                }
            }
            else if (IWidgetItem::m_OnHoverExit)
            {
                IWidgetItem::m_OnHoverExit();
            }
        }

        hovered = contains;

        if (contains && isPressed && !pressed)
        {
            pressed = true;
            if (IWidgetItem::m_OnPressed)
            {
                IWidgetItem::m_OnPressed();
            }
            return;
        }

        if (pressed && !isPressed)
        {
            pressed = false;
            if (contains && IWidgetItem::m_OnClick)
            {
                IWidgetItem::m_OnClick();
            }

            if (IWidgetItem::m_OnReleased)
            {
                IWidgetItem::m_OnReleased();
            }
        }
    }

    void WidgetButton::Measure()
    {
        if (label)
        {
            label->Measure();
        }
    }

    void WidgetButton::Arrange(const Rect &parentRect)
    {
        worldRect = CalculateAlignedRect(parentRect);
    }

    bool WidgetButton::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }
}
