// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef NODE_GRAPH_HPP
#define NODE_GRAPH_HPP

#include <imgui.h>

namespace ignite::UI
{
    struct NodeGraphStyle
    {
        float minZoom = 0.35f;
        float maxZoom = 2.5f;
        float gridStep = 32.0f;
        float cornerRounding = 0.0f;
        ImU32 backgroundColor = IM_COL32(24, 27, 33, 255);
        ImU32 borderColor = IM_COL32(58, 64, 74, 255);
        ImU32 gridColor = IM_COL32(44, 49, 58, 180);
    };

    struct NodeGraphState
    {
        ImVec2 pan = ImVec2(36.0f, 36.0f);
        float zoom = 1.0f;
    };

    struct NodeGraphCanvas
    {
        const char *id = nullptr;
        ImGuiID targetId = 0;
        ImVec2 screenPos = ImVec2(0.0f, 0.0f);
        ImVec2 size = ImVec2(0.0f, 0.0f);
        ImDrawList *drawList = nullptr;
        NodeGraphState *state = nullptr;
        NodeGraphStyle style = {};
        bool hovered = false;
        bool active = false;
    };

    struct NodeGraphNode
    {
        ImVec2 worldPos = ImVec2(0.0f, 0.0f);
        ImVec2 worldSize = ImVec2(0.0f, 0.0f);
        ImVec2 screenMin = ImVec2(0.0f, 0.0f);
        ImVec2 screenMax = ImVec2(0.0f, 0.0f);
    };

    struct NodeInteraction
    {
        bool hovered = false;
        bool clicked = false;
        bool active = false;
        bool dragging = false;
        ImVec2 dragDeltaWorld = ImVec2(0.0f, 0.0f);
    };

    class NodeGraph
    {
    public:
        static NodeGraphCanvas BeginCanvas(const char *id, NodeGraphState &state, const NodeGraphStyle &style = {});
        static void EndCanvas(const NodeGraphCanvas &canvas);

        static ImVec2 ToScreen(const NodeGraphCanvas &canvas, const ImVec2 &worldPos);
        static ImVec2 ToWorld(const NodeGraphCanvas &canvas, const ImVec2 &screenPos);
        static float ToScreenScale(const NodeGraphCanvas &canvas, float worldScale);

        static NodeGraphNode BuildNode(const NodeGraphCanvas &canvas, const ImVec2 &worldPos, const ImVec2 &worldSize);
        static NodeInteraction HandleNode(const NodeGraphCanvas &canvas, const char *id, const NodeGraphNode &node, int mouseButton = ImGuiMouseButton_Left);

        static void DrawConnection(const NodeGraphCanvas &canvas, const ImVec2 &fromWorldPos, const ImVec2 &toWorldPos, ImU32 color, float thickness = 2.0f);
        static bool BeginDropTarget(const NodeGraphCanvas &canvas);
        static void EndDropTarget();
    };
}

#endif
