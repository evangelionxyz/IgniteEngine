// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_image.hpp"

namespace ignite
{
    WidgetImage::WidgetImage(WidgetID wID)
        : IWidgetItem(wID)
    {
        size = glm::vec2(200.0f, 200.0f);
    }

    WidgetImage::~WidgetImage()
    {
        image = nullptr;
    }

    void WidgetImage::Arrange(const Rect &parentArea)
    {
        worldRect = CalculateAlignedRect(parentArea);
    }

    bool WidgetImage::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }
}
