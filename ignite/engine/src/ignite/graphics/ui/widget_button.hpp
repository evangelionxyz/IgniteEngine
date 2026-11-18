// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WIDGET_BUTTON_HPP
#define IGN_WIDGET_BUTTON_HPP

#include "widget.hpp"

namespace ignite
{
    class WidgetLabel;

    struct ButtonStyle
    {
        glm::vec4 color;
        glm::vec4 hoverColor;
        glm::vec4 pressedColor;
        glm::vec4 disabledColor = { 0.22f, 0.22f, 0.25f, 0.65f };
        glm::vec4 borderColor;
        float cornerRadius = 0.0f;
        float borderWidth = 1.0f;
    };

    class IGN_API WidgetButton : public IWidgetItem
    {
    public:
        bool hovered = false;
        bool pressed = false;
        float hoverAnim = 0.0f;
        float pressAnim = 0.0f;

        ButtonStyle style;

        AssetHandle imageHandle = AssetHandle(0);
        Ref<Texture> image = nullptr;
        Scope<WidgetLabel> label;

    public:
        WidgetButton(const std::string &text, WidgetID wID);
        virtual ~WidgetButton() override;
        glm::vec4 GetCurrentColor() const;
        void UpdateAnimation(float dt);

        void ResetInteractionState();
        void OnMouseClick(const glm::uvec2 &mousePos, bool isPressed);

        virtual void Measure() override;
        virtual void Arrange(const Rect &) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Button; }
    };
}

#endif
