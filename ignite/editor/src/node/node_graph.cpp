// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "node_graph.hpp"

#include <algorithm>
#include <cmath>

#include <imgui_internal.h>

namespace ignite::UI
{
    namespace
    {
        static ImVec2 Add(const ImVec2 &a, const ImVec2 &b)
        {
            return ImVec2(a.x + b.x, a.y + b.y);
        }

        static ImVec2 Sub(const ImVec2 &a, const ImVec2 &b)
        {
            return ImVec2(a.x - b.x, a.y - b.y);
        }

        static ImVec2 Mul(const ImVec2 &v, float s)
        {
            return ImVec2(v.x * s, v.y * s);
        }
    }

    NodeGraphCanvas NodeGraph::BeginCanvas(const char *id, NodeGraphState &state, const NodeGraphStyle &style)
    {
        NodeGraphCanvas canvas;
        canvas.id = id;
        canvas.state = &state;
        canvas.style = style;
        canvas.screenPos = ImGui::GetCursorScreenPos();
        canvas.size = ImGui::GetContentRegionAvail();
        canvas.size.x = std::max(canvas.size.x, 120.0f);
        canvas.size.y = std::max(canvas.size.y, 120.0f);
        canvas.drawList = ImGui::GetWindowDrawList();

		// Draw background and grid
        const ImVec2 screenEnd = Add(canvas.screenPos, canvas.size);
        canvas.drawList->AddRectFilled(canvas.screenPos, screenEnd, style.backgroundColor, style.cornerRounding);
        canvas.drawList->AddRect(canvas.screenPos, screenEnd, style.borderColor, style.cornerRounding, ImDrawFlags_None, 1.5f);

        const float scaledGrid = std::max(style.gridStep * state.zoom, 8.0f);
        for (float x = std::fmod(state.pan.x, scaledGrid); x < canvas.size.x; x += scaledGrid)
        {
            canvas.drawList->AddLine(ImVec2(canvas.screenPos.x + x, canvas.screenPos.y), ImVec2(canvas.screenPos.x + x, screenEnd.y), style.gridColor);
        }
        for (float y = std::fmod(state.pan.y, scaledGrid); y < canvas.size.y; y += scaledGrid)
        {
            canvas.drawList->AddLine(ImVec2(canvas.screenPos.x, canvas.screenPos.y + y), ImVec2(screenEnd.x, canvas.screenPos.y + y), style.gridColor);
        }

        ImGui::InvisibleButton(id, canvas.size, ImGuiButtonFlags_MouseButtonMiddle);
        canvas.targetId = ImGui::GetItemID();
        canvas.hovered = ImGui::IsItemHovered();
        canvas.active = ImGui::IsItemActive();

        if (canvas.hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            state.pan.x += ImGui::GetIO().MouseDelta.x;
            state.pan.y += ImGui::GetIO().MouseDelta.y;
        }

        if (canvas.hovered)
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                const float oldZoom = state.zoom;
                const float newZoom = std::clamp(oldZoom * std::pow(1.1f, wheel), style.minZoom, style.maxZoom);
                if (newZoom != oldZoom)
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    const ImVec2 mouseLocal = Sub(mouse, canvas.screenPos);
                    const ImVec2 worldBefore((mouseLocal.x - state.pan.x) / oldZoom, (mouseLocal.y - state.pan.y) / oldZoom);
                    state.zoom = newZoom;
                    state.pan.x = mouseLocal.x - worldBefore.x * newZoom;
                    state.pan.y = mouseLocal.y - worldBefore.y * newZoom;
                }
            }
        }

        return canvas;
    }

    void NodeGraph::EndCanvas(const NodeGraphCanvas &canvas)
    {
        const ImVec2 screenEnd = Add(canvas.screenPos, canvas.size);
        ImGui::GetWindowDrawList()->PushClipRect(canvas.screenPos, screenEnd, true);
        ImGui::GetWindowDrawList()->PopClipRect();
    }

    ImVec2 NodeGraph::ToScreen(const NodeGraphCanvas &canvas, const ImVec2 &worldPos)
    {
        return { canvas.screenPos.x + canvas.state->pan.x + worldPos.x * canvas.state->zoom,
            canvas.screenPos.y + canvas.state->pan.y + worldPos.y * canvas.state->zoom };
    }

    ImVec2 NodeGraph::ToWorld(const NodeGraphCanvas &canvas, const ImVec2 &screenPos)
    {
        return { (screenPos.x - canvas.screenPos.x - canvas.state->pan.x) / canvas.state->zoom,
            (screenPos.y - canvas.screenPos.y - canvas.state->pan.y) / canvas.state->zoom };
    }

    float NodeGraph::ToScreenScale(const NodeGraphCanvas &canvas, float worldScale)
    {
        return worldScale * canvas.state->zoom;
    }

    NodeGraphNode NodeGraph::BuildNode(const NodeGraphCanvas &canvas, const ImVec2 &worldPos, const ImVec2 &worldSize)
    {
        NodeGraphNode node;
        node.worldPos = worldPos;
        node.worldSize = worldSize;
        node.screenMin = ToScreen(canvas, worldPos);
        node.screenMax = Add(node.screenMin, Mul(worldSize, canvas.state->zoom));
        return node;
    }

    NodeInteraction NodeGraph::HandleNode(const NodeGraphCanvas &canvas, const char *id, const NodeGraphNode &node, int mouseButton)
    {
        NodeInteraction interaction;
        ImGui::SetCursorScreenPos(node.screenMin);
        ImGui::InvisibleButton(id, Sub(node.screenMax, node.screenMin), mouseButton == ImGuiMouseButton_Middle ? ImGuiButtonFlags_MouseButtonMiddle : ImGuiButtonFlags_None);
        interaction.hovered = ImGui::IsItemHovered();
        interaction.clicked = ImGui::IsItemClicked(mouseButton);
        interaction.active = ImGui::IsItemActive();
        interaction.dragging = ImGui::IsMouseDragging(mouseButton);
        interaction.dragDeltaWorld = ImVec2(ImGui::GetIO().MouseDelta.x / canvas.state->zoom, ImGui::GetIO().MouseDelta.y / canvas.state->zoom);
        return interaction;
    }

    void NodeGraph::DrawConnection(const NodeGraphCanvas &canvas, const ImVec2 &fromWorldPos, const ImVec2 &toWorldPos, ImU32 color, float thickness)
    {
        const ImVec2 start = ToScreen(canvas, fromWorldPos);
        const ImVec2 end = ToScreen(canvas, toWorldPos);
        const float handleOffset = 70.0f * canvas.state->zoom;
        canvas.drawList->AddBezierCubic(start, ImVec2(start.x + handleOffset, start.y), ImVec2(end.x - handleOffset, end.y), end, color, thickness);
    }

    bool NodeGraph::BeginDropTarget(const NodeGraphCanvas &canvas)
    {
        const ImRect rect(canvas.screenPos, Add(canvas.screenPos, canvas.size));
        const ImGuiID targetId = canvas.targetId != 0 ? canvas.targetId : ImGui::GetID(canvas.id);
        return ImGui::BeginDragDropTargetCustom(rect, targetId);
    }

    void NodeGraph::EndDropTarget()
    {
        ImGui::EndDragDropTarget();
    }
}
