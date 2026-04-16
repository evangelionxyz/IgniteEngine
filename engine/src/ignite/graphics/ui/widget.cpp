// Copyright (c) 2026 Evangelion Manuhutu

#include "widget.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
    IWidgetItem::IWidgetItem(Widget *widget)
        : m_Widget(widget)
    {

    }

    Rect IWidgetItem::GetAlignedRect() const
    {
        Rect alignedRect = m_Rect;
        glm::vec2 &position = alignedRect.min;
        glm::vec2 &size = alignedRect.max;
        
        const auto &vpSizeI = m_Widget->GetViewportSize();
        const glm::vec2 viewportSize = { static_cast<float>(vpSizeI.x), static_cast<float>(vpSizeI.y) };
        
        switch (m_Alignment)
        {
            case WidgetAlignment::TOP_CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
            break;
            case WidgetAlignment::TOP_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x;
                break;
            case WidgetAlignment::CENTER_LEFT:
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case WidgetAlignment::CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case WidgetAlignment::CENTER_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x;
                position.y = viewportSize.y / 2.0f + m_Rect.min.y - m_Rect.GetSize().y / 2.0f;
                size.y = viewportSize.y / 2.0f + m_Rect.min.y + m_Rect.GetSize().y / 2.0f;
                break;
            case WidgetAlignment::BOTTOM_LEFT:
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case WidgetAlignment::BOTTOM_CENTER:
                position.x = viewportSize.x / 2.0f + m_Rect.min.x - m_Rect.GetSize().x / 2.0f;
                size.x = viewportSize.x / 2.0f + m_Rect.min.x + m_Rect.GetSize().x / 2.0f;
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case WidgetAlignment::BOTTOM_RIGHT:
                position.x = viewportSize.x - m_Rect.min.x - m_Rect.GetSize().x;
                size.x = viewportSize.x - m_Rect.min.x; 
                position.y = viewportSize.y - m_Rect.max.y;
                size.y = viewportSize.y - m_Rect.min.y;
                break;
            case WidgetAlignment::TOP_LEFT:
            default: break;
        }

        return alignedRect;
    }

    WidgetButton::WidgetButton(Widget *widget, const std::string &text)
        : IWidgetItem(widget), m_Text(text)
    {
        m_NormalColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
        m_HoverColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        m_PressedColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        m_TextColor = UI_COLOR_WHITE;
        m_BorderColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
    }

    WidgetButton::~WidgetButton()
    {
    }

    void WidgetButton::Update(float deltaTime, const glm::uvec2 &mousePos)
    {
        bool wasHovered = m_IsHovered;
        m_IsHovered = Contains(mousePos);

        if (m_IsHovered && !wasHovered && m_OnHoverEnter)
        {
            m_OnHoverEnter();
        }
        else if (!m_IsHovered && wasHovered && m_OnHoverExit)
        {
            m_OnHoverExit();
        }
    }

    void WidgetButton::OnMouseClick(const glm::uvec2 &mousePos, bool isPressed)
    {
        if (Contains(mousePos))
        {
            if (isPressed)
            {
                m_IsPressed = true;
                if (m_OnPressed)
                {
                    m_OnPressed();
                }
            }
            else if (m_IsPressed)
            {
                m_IsPressed = false;
                if (m_OnClick)
                {
                    m_OnClick();
                }
                if (m_OnReleased)
                {
                    m_OnReleased();
                }
            }
        }
        else if (!isPressed)
        {
            m_IsPressed = false;
        }
    }

    Widget::Widget(Scene *scene)
        : m_Scene(scene), m_ViewportSize({ 1280, 720 })
    {
    }

    Widget::~Widget()
    {
    }

    void Widget::Update(float deltaTime, const glm::uvec2 &mousePos)
    {
        for (auto &[id, item] : m_WidgetItems)
        {
            item->Update(deltaTime, mousePos);
        }
    }

    bool Widget::Serialize(const std::filesystem::path &filepath)
    {
        return true;
    }

    Ref<Widget> Widget::Deserialize(const std::filesystem::path &filepath)
    {
        return nullptr;
    }

}
