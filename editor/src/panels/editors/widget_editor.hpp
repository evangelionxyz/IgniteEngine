// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef WIDGET_EDITOR_HPP
#define WIDGET_EDITOR_HPP

#include "ignite/graphics/ui/widget_container.hpp"

namespace ignite
{
    class WidgetEditor
    {
    public:
        static WidgetContainer *ResolveInsertionParent(const Ref<IWidgetItem> &selectedItem, const Ref<WidgetCanvas> &widget);
        static void DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas);
        static void DrawWidgetDetails(const Ref<WidgetCanvas> &widget, int &selectedItemId, AssetManager *assetManager);
    };
}

#endif