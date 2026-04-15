// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATION_TIMELINE_HPP
#define ANIMATION_TIMELINE_HPP

#include <algorithm>
#include <string>
#include <format>

#include <glm/glm.hpp>
#include <imgui.h>

namespace ignite::UI
{
    struct TimelineState
    {
        int frameCount;
        float fps;
        float totalDuration;
        float currentTime;
    };

    struct Timeline
    {
        static void Draw(ImDrawList *dl, float tlHeight, const TimelineState &tl, float *playbackTime, int *previewFrame, bool *isPlaying, const char *emptyMessage = "Select animation")
        {
            // Construct height
            const float rulerHeight = 16.0f; // Time label strip
            const float dotAreaHeight = tlHeight - rulerHeight;
            const ImVec2 tlPos = ImGui::GetCursorScreenPos();

            // Construct width
            const float tlWidth = ImGui::GetContentRegionAvail().x;
            
            // Make the timeline as a button so we can interact with it
            ImGui::InvisibleButton("##timeline_", { tlWidth, tlHeight });
            const bool isHovered = ImGui::IsItemHovered();
            const bool isActive = ImGui::IsItemActive();

            // Background
            dl->AddRectFilled(tlPos, ImVec2(tlPos.x + tlWidth, tlPos.y + tlHeight), IM_COL32(30, 30, 35, 255));
            dl->AddRect(tlPos, ImVec2(tlPos.x + tlWidth, tlPos.y + tlHeight), IM_COL32(70, 70, 80, 255));

            const float frameDuration = (tl.fps > 0) ? (1.0f / tl.fps) : 0.0f;

            // Draw foreground - frame label
            if (tl.frameCount > 0)
            {
                const float cellW = tlWidth / static_cast<float>(tl.frameCount);
                const int activeFrame = previewFrame ? *previewFrame : -1;

                // Ruler: time labels
                for (int i = 0; i < tl.frameCount; ++i)
                {
                    const float cx = tlPos.x + (i + 0.5f) * cellW;
                    const float timeAtFrame = i * frameDuration;
                    const std::string ts = std::format("{:.2f}s", timeAtFrame);
                    const ImVec2 tsz = ImGui::CalcTextSize(ts.c_str());

                    if (cellW >= tsz.x + 4.0f || i % std::max(1, static_cast<int>(tsz.x / cellW) + 1) == 0)
                    {
                        dl->AddText(ImVec2(cx - tsz.x * 0.5f, tlPos.y + 1.0f), IM_COL32(160, 160, 180, 200), ts.c_str());
                    }
                }

                // Horizontal divider under ruler
                dl->AddLine({ tlPos.x, tlPos.y + rulerHeight }, { tlPos.x + tlWidth, tlPos.y + rulerHeight }, IM_COL32(70, 70, 80, 255));

                // Frame dots
                const float dotY = tlPos.y + rulerHeight + dotAreaHeight * 0.5f;
                for (int i = 0; i < tl.frameCount; ++i)
                {
                    const float cx = tlPos.x + (i + 0.5f) * cellW;

                    // Cell separator tick
                    dl->AddLine(ImVec2(cx - cellW * 0.5f, tlPos.y + rulerHeight), ImVec2(cx - cellW * 0.5f, tlPos.y + tlHeight), IM_COL32(55, 55, 65, 255));

                    const bool isActive = (i == activeFrame);
                    const float dotR = isActive ? 7.0f : 5.0f;
                    const ImU32 dotCol = isActive ? IM_COL32(80, 200, 120, 255) : IM_COL32(120, 130, 160, 220);
                    dl->AddCircleFilled(ImVec2(cx, dotY), dotR, dotCol);
                    dl->AddCircle(ImVec2(cx, dotY), dotR, IM_COL32(200, 200, 220, 255), 0, 1.3f);
                }

                // Playhead line
                if (tl.totalDuration > 0.0f)
                {
                    const float phX = tlPos.x + (*playbackTime / tl.totalDuration) * tlWidth;
                    dl->AddLine(ImVec2(phX, tlPos.y), ImVec2(phX, tlPos.y + tlHeight), IM_COL32(255, 100, 60, 230), 2.0f);

                    // Playhead handle triangle
                    dl->AddTriangleFilled(ImVec2(phX - 5, tlPos.y), ImVec2(phX + 5, tlPos.y), ImVec2(phX, tlPos.y + 10), IM_COL32(255, 100, 60, 230));
                }

                // Scrubbing
                if ((isHovered || isActive) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    const float mx = std::clamp(ImGui::GetMousePos().x - tlPos.x, 0.0f, tlWidth);
                    *playbackTime = (mx / tlWidth) * tl.totalDuration;
                    if (previewFrame)
                    {
                        *previewFrame = std::min(static_cast<int>(*playbackTime * tl.fps), tl.frameCount - 1);
                    }
                    *isPlaying = false;
                }
            }
            else
            {
                const char *noMsg = emptyMessage ? emptyMessage : "Select animation";
                const ImVec2 ns = ImGui::CalcTextSize(noMsg);
                dl->AddText(ImVec2(tlPos.x + (tlWidth - ns.x) * 0.5f, tlPos.y + (tlHeight - ns.y) * 0.5f), IM_COL32(110, 110, 120, 200), noMsg);
            }
        }
    };
}

#endif