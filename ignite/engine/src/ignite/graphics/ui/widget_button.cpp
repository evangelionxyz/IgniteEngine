// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "widget_button.hpp"
#include "widget_label.hpp"

namespace ignite
{
    WidgetButton::WidgetButton(const std::string &text, WidgetID wID)
        : label(CreateScope<WidgetLabel>(text, (WidgetID)-1)), IWidgetItem(wID)
    {
        baseStyle.backgroundColor = UI_COLOR_DARK_GRAY;
        baseStyle.borderColor = UI_COLOR_GRAY;
        baseStyle.borderWidth = 1.0f;
        baseStyle.cornerRadius = 0.0f;
        baseStyle.shadowEnabled = true;
        baseStyle.shadowColor = glm::vec4(0.0f, 0.015f, 0.04f, 0.58f);
        baseStyle.shadowOffset = glm::vec2(1.0f, 5.0f);
        baseStyle.shadowSize = 6.0f;
        baseStyle.shadowSoftness = 0.34f;

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

    glm::vec4 WidgetButton::GetCurrentColor() const
    {
        if (!interactable)
            return style.disabledColor;

        glm::vec4 col = glm::mix(style.color, style.hoverColor, glm::clamp(hoverAnim, 0.0f, 1.0f));
        col = glm::mix(col, style.pressedColor, glm::clamp(pressAnim, 0.0f, 1.0f));
        return col;
    }

    void WidgetButton::UpdateAnimation(float dt)
    {
        const float safeDt = std::clamp(dt, 0.0f, 0.1f);
        const float targetHover = (interactable && hovered) ? 1.0f : 0.0f;
        const float targetPress = (interactable && pressed) ? 1.0f : 0.0f;
        hoverAnim = glm::mix(hoverAnim, targetHover, std::min(1.0f, safeDt * 16.0f));
        pressAnim = glm::mix(pressAnim, targetPress, std::min(1.0f, safeDt * 24.0f));
    }

    void WidgetButton::ResetInteractionState()
    {
        if (hovered && IWidgetItem::m_OnHoverExit)
        {
            IWidgetItem::m_OnHoverExit();
        }
        hovered = false;
        pressed = false;
        hoverAnim = 0.0f;
        pressAnim = 0.0f;
    }

    void WidgetButton::OnMouseClick(const glm::uvec2 &mousePos, bool isPressed)
    {
        if (!interactable || !IsEffectivelyVisible())
        {
            ResetInteractionState();
            return;
        }

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
        IWidgetItem::Arrange(parentRect);
        if (label)
        {
            label->Arrange(box.content);
        }
    }

    bool WidgetButton::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }
}
