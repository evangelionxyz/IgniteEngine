// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WIDGET_LABEL_HPP
#define IGN_WIDGET_LABEL_HPP

#include "widget.hpp"

namespace ignite
{
    struct LabelStyle
    {
        glm::vec4 color = glm::vec4(1.0f);
        float fontSize = 16.0f; // In pixel
        float kerning = 0.0f;
        float lineSpacing = 0.025f;
    };

    class IGN_API WidgetLabel : public IWidgetItem
    {
    public:
        AssetHandle fontHandle = AssetHandle(0);
        Ref<Font> font;
        std::string text;

        LabelStyle style;

    public:
        WidgetLabel(const std::string &text, WidgetID wID);
        virtual ~WidgetLabel() override;

        Rect GetTextBounds() const;
        virtual void Measure() override;
        virtual void Arrange(const Rect &) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Label; }
    };
}

#endif