// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_IMAGE_HPP
#define WIDGET_IMAGE_HPP

#include "widget.hpp"

namespace ignite
{
    class WidgetImage : public IWidgetItem
    {
    public:
        WidgetImage(WidgetID wID);
        virtual ~WidgetImage() override;

        AssetHandle imageHandle;
        Ref<Texture> image;

    };
}
#endif
