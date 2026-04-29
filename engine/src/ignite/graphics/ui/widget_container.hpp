// Copyright (c) 2026 Evangelion Manuhutu

// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_CONTAINER_HPP
#define WIDGET_CONTAINER_HPP

#include "widget.hpp"

namespace ignite
{
    class WidgetContainer : public IWidgetItem
    {
    public:
        WidgetContainer(WidgetID wID);
        virtual ~WidgetContainer() override;

        LayoutMode layout = LayoutMode::Vertical;
        float gap = 0.0f;

        virtual void Measure() override;
        virtual void Arrange(const Rect &parentArea) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Container; }
    };
}

#endif