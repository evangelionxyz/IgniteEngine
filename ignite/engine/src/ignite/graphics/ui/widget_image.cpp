// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "widget_image.hpp"

namespace ignite
{
    WidgetImage::WidgetImage(WidgetID wID)
        : IWidgetItem(wID)
    {
        layout.width = 200.0f;
        layout.height = 200.0f;
    }

    WidgetImage::~WidgetImage()
    {
        image = nullptr;
    }

    void WidgetImage::Arrange(const Rect &parentArea)
    {
        IWidgetItem::Arrange(parentArea);
    }

    bool WidgetImage::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }
}
