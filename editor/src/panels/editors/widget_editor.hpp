// Copyright (c) 2026 Evangelion Manuhutu

// Created by: Evangelion Manuhutu
// Date      : 19 April 2026

#pragma once
#ifndef WIDGET_EDITOR_HPP
#define WIDGET_EDITOR_HPP

#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/graphics/renderer/asset_scene_renderer.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

namespace ignite
{
    // Drag-and-drop payload type for the toolbox panel
    static constexpr const char *DND_WIDGET_TOOLBOX_ITEM = "WIDGET_TOOLBOX_ITEM";

    class WidgetEditor
    {
    public:
        // --- Tree ---
        static WidgetContainer *ResolveInsertionParent(const Ref<IWidgetItem> &selectedItem, const Ref<WidgetCanvas> &widget);
        static void DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas);

        // --- Details ---
        static void DrawWidgetDetails(const Ref<WidgetCanvas> &widget, int &selectedItemId, AssetManager *assetManager);

        // --- Toolbox ---
        // Renders draggable widget-type buttons that can be dropped onto the preview viewport.
        static void DrawToolbox(const Ref<WidgetCanvas> &widget, int &selectedItemId);

        // --- Preview overlays ---
        // Draws bounds rectangles + corner handles for all items over the preview viewport.
        // imagePos/imageSize: screen-space position and size of the rendered canvas image.
        // canvasW/H: logical canvas dimensions (widget viewport size).
        static void DrawPreviewOverlay(
            ImDrawList *drawList,
            const Ref<WidgetCanvas> &widget,
            int selectedItemId,
            const ImVec2 &imagePos,
            const ImVec2 &imageSize,
            float canvasW, float canvasH);

        // Draws 9 anchor-point diamonds on the selected item's parent rect,
        // highlighting the one matching the item's current alignment setting.
        static void DrawAnchorPoints(
            ImDrawList *drawList,
            const Ref<IWidgetItem> &item,
            const Ref<WidgetCanvas> &widget,
            const ImVec2 &imagePos,
            const ImVec2 &imageSize,
            float canvasW, float canvasH);

        // Convert a canvas-space coordinate to ImGui screen space.
        static ImVec2 CanvasToScreen(float cx, float cy,
            const ImVec2 &imagePos, const ImVec2 &imageSize,
            float canvasW, float canvasH);
    };
}

#endif