// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WIDGET_IMAGE_HPP
#define IGN_WIDGET_IMAGE_HPP

#include "widget.hpp"

namespace ignite
{
    class IGN_API WidgetImage : public IWidgetItem
    {
    public:
        WidgetImage(WidgetID wID);
        virtual ~WidgetImage() override;

        AssetHandle imageHandle = AssetHandle(0);
        Ref<Texture> image;
        glm::vec4 tintColor = glm::vec4(1.0f);
        glm::vec2 uv0 = glm::vec2(0.0f);
        glm::vec2 uv1 = glm::vec2(1.0f);

        virtual void Arrange(const Rect &parentArea) override;
        virtual bool HitTest(int px, int py) override;

        virtual WidgetType GetWidgetType() const override { return WidgetType::Image; }
    };
}
#endif
