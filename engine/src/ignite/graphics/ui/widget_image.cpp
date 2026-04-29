// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_image.hpp"

namespace ignite
{

    WidgetImage::WidgetImage(WidgetID wID)
        : IWidgetItem(wID)
    {
    }

    WidgetImage::~WidgetImage()
    {
        image = nullptr;
    }

}
