// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_BUTTON_HPP
#define WIDGET_BUTTON_HPP

#include "widget.hpp"

namespace ignite
{
    class WidgetLabel;

    struct ButtonStyle
    {
        glm::vec4 color;
        glm::vec4 hoverColor;
        glm::vec4 pressedColor;
        glm::vec4 borderColor;
        float cornerRadius = 0.0f;
        float borderWidth = 1.0f;
    };

    class WidgetButton : public IWidgetItem
    {
    public:
        bool hovered = false;
        bool pressed = false;

        ButtonStyle style;

        AssetHandle imageHandle = AssetHandle(0);
        Ref<Texture> image = nullptr;
        Scope<WidgetLabel> label;

    public:
        WidgetButton(const std::string &text, WidgetID wID);
        virtual ~WidgetButton() override;
        const glm::vec4 &GetCurrentColor() const;

        void OnMouseClick(const glm::uvec2 &mousePos, bool isPressed);

        virtual void Measure() override;
        virtual void Arrange(const Rect &) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Button; }

    private:
        std::function<void()> m_OnClick;
        std::function<void()> m_OnPressed;
        std::function<void()> m_OnReleased;
        std::function<void()> m_OnHoverEnter;
        std::function<void()> m_OnHoverExit;
    };
}

#endif
