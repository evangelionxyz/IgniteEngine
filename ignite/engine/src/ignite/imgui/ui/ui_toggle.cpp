// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_toggle.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ignite::UI
{
    bool Switch(const char *label, bool *value, const char *description, bool disabled)
    {
        if (!value)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);

        const char *safeLabel = label ? label : "##switch";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const bool hasLabel = (labelEnd > safeLabel);
        const bool hasDesc = (description && description[0] != '\0');

        const float trackW = std::round(m.switchWidth * scale);
        const float trackH = std::round(m.switchHeight * scale);
        const float gap = std::round(10.0f * scale);

        const ImVec2 labelSize = hasLabel ? ImGui::CalcTextSize(safeLabel, labelEnd, false) : ImVec2(0.0f, 0.0f);
        ImVec2 descSize(0.0f, 0.0f);
        if (hasDesc)
        {
            ScopedFont smallFont(FontVariant::Small);
            descSize = ImGui::CalcTextSize(description);
        }

        const float textW = std::max(labelSize.x, descSize.x);
        const float textH = (hasLabel ? labelSize.y : 0.0f) + (hasDesc ? (descSize.y + 2.0f * scale) : 0.0f);
        const float totalW = trackW + ((hasLabel || hasDesc) ? (gap + textW) : 0.0f);
        const float totalH = std::max(trackH, textH);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        if (disabled)
            ImGui::BeginDisabled(true);

        const bool clicked = ImGui::InvisibleButton(safeLabel, ImVec2(totalW, totalH));
        const bool isHovered = !disabled && ImGui::IsItemHovered();
        const bool isFocused = !disabled && ImGui::IsItemFocused();

        if (clicked && !disabled)
            *value = !(*value);

        if (isHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const ImGuiID id = ImGui::GetItemID();
        const float anim = AnimateFloat(id, *value ? 1.0f : 0.0f, 20.0f);

        const float trackY = origin.y + std::round((totalH - trackH) * 0.5f);
        const ImVec2 trackMin(origin.x, trackY);
        const ImVec2 trackMax(origin.x + trackW, trackY + trackH);

        const glm::vec4 trackBg = LerpColor(t.secondary, t.primary, anim);
        const glm::vec4 trackBorder = LerpColor(isHovered ? t.borderStrong : t.border, t.primary, anim);
        const glm::vec4 thumbCol = LerpColor(t.mutedForeground, t.primaryForeground, anim);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(trackMin, trackMax, ToU32(trackBg), 0.0f);
        dl->AddRect(trackMin, trackMax, ToU32(trackBorder), 0.0f, 0, 1.0f);

        if (isFocused)
        {
            dl->AddRect(ImVec2(trackMin.x - 1.0f, trackMin.y - 1.0f), ImVec2(trackMax.x + 1.0f, trackMax.y + 1.0f), ToU32(t.ring, 0.85f), 0.0f, 0, 1.0f);
        }

        const float thumbInset = std::round(3.0f * scale);
        const float thumbSize = trackH - thumbInset * 2.0f;
        const float thumbTravel = trackW - thumbInset * 2.0f - thumbSize;
        const float thumbX = trackMin.x + thumbInset + std::round(thumbTravel * anim);
        const float thumbY = trackMin.y + thumbInset;
        dl->AddRectFilled(ImVec2(thumbX, thumbY), ImVec2(thumbX + thumbSize, thumbY + thumbSize), ToU32(thumbCol), 0.0f);

        if (hasLabel || hasDesc)
        {
            const float textX = trackMax.x + gap;
            float textY = origin.y + std::round((totalH - textH) * 0.5f);
            if (hasLabel)
            {
                dl->AddText(ImVec2(textX, textY), ToU32(disabled ? t.disabledForeground : t.foreground), safeLabel, labelEnd);
                textY += labelSize.y + 2.0f * scale;
            }
            if (hasDesc)
            {
                ScopedFont smallFont(FontVariant::Small);
                dl->AddText(ImVec2(textX, textY), ToU32(t.mutedForeground), description);
            }
        }

        if (disabled)
            ImGui::EndDisabled();

        return clicked && !disabled;
    }

    bool Checkbox(const char *label, bool *value, const char *description, bool indeterminate)
    {
        if (!value)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);

        const char *safeLabel = label ? label : "##checkbox";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const bool hasLabel = (labelEnd > safeLabel);
        const bool hasDesc = (description && description[0] != '\0');

        const float boxSize = std::round(m.checkboxSize * scale);
        const float gap = std::round(8.0f * scale);

        const ImVec2 labelSize = hasLabel ? ImGui::CalcTextSize(safeLabel, labelEnd, false) : ImVec2(0.0f, 0.0f);
        ImVec2 descSize(0.0f, 0.0f);
        if (hasDesc)
        {
            ScopedFont smallFont(FontVariant::Small);
            descSize = ImGui::CalcTextSize(description);
        }

        const float textW = std::max(labelSize.x, descSize.x);
        const float textH = (hasLabel ? labelSize.y : 0.0f) + (hasDesc ? (descSize.y + 2.0f * scale) : 0.0f);
        const float totalW = boxSize + ((hasLabel || hasDesc) ? (gap + textW) : 0.0f);
        const float totalH = std::max(boxSize, textH);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton(safeLabel, ImVec2(totalW, totalH));
        const bool isHovered = ImGui::IsItemHovered();
        const bool isFocused = ImGui::IsItemFocused();

        if (clicked)
            *value = !(*value);

        if (isHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const bool isChecked = (*value || indeterminate);
        const ImGuiID id = ImGui::GetItemID();
        const float anim = AnimateFloat(id, isChecked ? 1.0f : 0.0f, 22.0f);

        const float boxY = origin.y + (hasDesc ? std::round((labelSize.y - boxSize) * 0.5f) : std::round((totalH - boxSize) * 0.5f));
        const ImVec2 boxMin(origin.x, std::max(origin.y, boxY));
        const ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);

        const glm::vec4 bgCol = LerpColor(t.inputBg, t.primary, anim);
        const glm::vec4 borderCol = LerpColor(isHovered ? t.borderStrong : t.borderStrong, t.primary, anim);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(boxMin, boxMax, ToU32(bgCol), 0.0f);
        dl->AddRect(boxMin, boxMax, ToU32(borderCol), 0.0f, 0, 1.0f);

        if (isFocused)
        {
            dl->AddRect(ImVec2(boxMin.x - 1.0f, boxMin.y - 1.0f), ImVec2(boxMax.x + 1.0f, boxMax.y + 1.0f), ToU32(t.ring, 0.85f), 0.0f, 0, 1.0f);
        }

        if (anim > 0.1f)
        {
            const ImU32 markCol = ToU32(t.primaryForeground, anim);
            if (indeterminate)
            {
                const float yMid = std::round((boxMin.y + boxMax.y) * 0.5f);
                const float xPad = std::round(boxSize * 0.25f);
                dl->AddLine(ImVec2(boxMin.x + xPad, yMid), ImVec2(boxMax.x - xPad, yMid), markCol, 2.0f * scale);
            }
            else
            {
                const ImVec2 p1(boxMin.x + boxSize * 0.24f, boxMin.y + boxSize * 0.52f);
                const ImVec2 p2(boxMin.x + boxSize * 0.44f, boxMin.y + boxSize * 0.72f);
                const ImVec2 p3(boxMin.x + boxSize * 0.78f, boxMin.y + boxSize * 0.30f);
                const ImVec2 pts[3] = { p1, p2, p3 };
                dl->AddPolyline(pts, 3, markCol, ImDrawFlags_None, 2.0f * scale);
            }
        }

        if (hasLabel || hasDesc)
        {
            const float textX = boxMax.x + gap;
            float textY = hasDesc ? origin.y : (origin.y + std::round((totalH - textH) * 0.5f));
            if (hasLabel)
            {
                dl->AddText(ImVec2(textX, textY), ToU32(t.foreground), safeLabel, labelEnd);
                textY += labelSize.y + 2.0f * scale;
            }
            if (hasDesc)
            {
                ScopedFont smallFont(FontVariant::Small);
                dl->AddText(ImVec2(textX, textY), ToU32(t.mutedForeground), description);
            }
        }

        return clicked;
    }

    bool RadioGroup(const char *id, std::span<const char *const> options, int *selectedIndex, bool horizontal)
    {
        if (options.empty() || !selectedIndex)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float circleR = std::round(8.0f * scale);
        const float dotR = std::round(3.5f * scale);
        const float gap = std::round(8.0f * scale);

        bool changed = false;
        ImGui::PushID(id);
        ImDrawList *dl = ImGui::GetWindowDrawList();

        for (int i = 0; i < static_cast<int>(options.size()); ++i)
        {
            if (horizontal && i > 0)
                ImGui::SameLine(0.0f, std::round(16.0f * scale));

            const char *opt = options[static_cast<size_t>(i)] ? options[static_cast<size_t>(i)] : "";
            const ImVec2 ts = ImGui::CalcTextSize(opt);
            const float itemW = circleR * 2.0f + gap + ts.x;
            const float itemH = std::max(circleR * 2.0f, ts.y) + std::round(4.0f * scale);

            ImGui::PushID(i);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##radio", ImVec2(itemW, itemH)))
            {
                if (*selectedIndex != i)
                {
                    *selectedIndex = i;
                    changed = true;
                }
            }
            const bool hovered = ImGui::IsItemHovered();
            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            const bool selected = (*selectedIndex == i);
            const ImVec2 center(pos.x + circleR, pos.y + itemH * 0.5f);

            dl->AddCircleFilled(center, circleR, ToU32(t.inputBg), 20);
            dl->AddCircle(center, circleR, ToU32(selected ? t.primary : (hovered ? t.borderStrong : t.border)), 20, 1.0f);
            if (selected)
            {
                dl->AddCircleFilled(center, dotR, ToU32(t.primary), 16);
            }

            const float textY = pos.y + std::round((itemH - ts.y) * 0.5f);
            dl->AddText(ImVec2(pos.x + circleR * 2.0f + gap, textY), ToU32(selected ? t.foreground : t.mutedForeground), opt);
            ImGui::PopID();
        }

        ImGui::PopID();
        return changed;
    }
}

