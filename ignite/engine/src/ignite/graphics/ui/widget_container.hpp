// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WIDGET_CONTAINER_HPP
#define IGN_WIDGET_CONTAINER_HPP

#include "widget.hpp"

namespace ignite
{
    class IGN_API WidgetContainer : public IWidgetItem
    {
    public:
        WidgetContainer(WidgetID wID);
        virtual ~WidgetContainer() override;

        virtual void Measure() override;
        virtual void Arrange(const Rect &parentArea) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Container; }
    };

    // A container that clamps its own size to [minSize, maxSize] before layout.
    class IGN_API WidgetBoxSizing : public WidgetContainer
    {
    public:
        WidgetBoxSizing(WidgetID wID);
        virtual ~WidgetBoxSizing() override;

        glm::vec2 minSize = glm::vec2(0.0f);
        glm::vec2 maxSize = glm::vec2(FLT_MAX);

        virtual void Measure() override;
        virtual WidgetType GetWidgetType() const override { return WidgetType::BoxSizing; }
    };

    // A container that stacks all children on top of each other (each child fills the full parent area).
    class IGN_API WidgetOverlay : public WidgetContainer
    {
    public:
        WidgetOverlay(WidgetID wID);
        virtual ~WidgetOverlay() override;

        virtual void Arrange(const Rect &parentArea) override;
        virtual WidgetType GetWidgetType() const override { return WidgetType::Overlay; }
    };
}

#endif
