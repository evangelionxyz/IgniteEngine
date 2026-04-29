// Copyright (c) 2026 Evangelion Manuhutu

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
        static WidgetContainer *ResolveInsertionParent(const Ref<IWidgetItem> &selectedItem, const Ref<WidgetCanvas> &widget);
        static void DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas);

        static void DrawWidgetDetails(const Ref<WidgetCanvas> &widget, int &selectedItemId, AssetManager *assetManager);
        static void DrawToolbox(const Ref<WidgetCanvas> &widget, int &selectedItemId);
        static void DrawPreviewOverlay(ImDrawList *drawList, const Ref<WidgetCanvas> &widget, int selectedItemId, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH);
        static void DrawAnchorPoints(ImDrawList *drawList, const Ref<IWidgetItem> &item, const Ref<WidgetCanvas> &widget, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH);

        // Convert a canvas-space coordinate to ImGui screen space.
        static ImVec2 CanvasToScreen(float cx, float cy, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH);

        // Widgets
        static bool DrawWidgetLabel(class WidgetLabel *label, AssetManager *assetManager);
        static bool DrawWidgetImage(class WidgetImage *image, AssetManager *assetManager);
        // Moved UI: full widget editor window for AssetEditorPanel to call
        static void UIWidgetEditor(class AssetEditorData &assetData, class EditorLayer *editorLayer);
    };
}

#endif