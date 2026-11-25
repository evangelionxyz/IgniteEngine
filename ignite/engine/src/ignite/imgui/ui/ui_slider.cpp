// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_slider.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ignite::UI
{
    namespace
    {
        bool DrawSliderFloatHeader(const char *label, const char *labelEnd, const char *valueText, float w, float scale)
        {
            if (labelEnd <= label)
                return false;

            const ThemeColors &t = GetTheme();
            const ImVec2 headerPos = ImGui::GetCursorScreenPos();
            const ImVec2 labelSize = ImGui::CalcTextSize(label, labelEnd, false);

            ImVec2 valSize(0.0f, 0.0f);
            {
                ScopedFont mono(FontVariant::Mono);
                valSize = ImGui::CalcTextSize(valueText);
            }

            const float headerH = std::max(labelSize.y, valSize.y) + std::round(4.0f * scale);
            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddText(headerPos, ToU32(t.foreground), label, labelEnd);
            {
                ScopedFont mono(FontVariant::Mono);
                dl->AddText(ImVec2(headerPos.x + w - valSize.x, headerPos.y), ToU32(t.mutedForeground), valueText);
            }
            ImGui::Dummy(ImVec2(w, headerH));
            return true;
        }
    }

    bool SliderFloat(const char *label, float *value, float minVal, float maxVal, const char *format, float width)
    {
        if (!value)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float w = (width > 0.0f) ? width : std::max(80.0f, ImGui::GetContentRegionAvail().x);

        const char *safeLabel = label ? label : "##slider";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const char *fmt = format ? format : "%.2f";

        char valBuf[64]{};
        std::snprintf(valBuf, sizeof(valBuf), fmt, *value);

        ImGui::PushID(safeLabel);
        const bool hasHeader = DrawSliderFloatHeader(safeLabel, labelEnd, valBuf, w, scale);

        const ImGuiID editStateId = ImGui::GetID("##editing");
        ImGuiStorage *storage = ImGui::GetStateStorage();
        bool isEditingText = storage->GetBool(editStateId, false);

        const float interactH = hasHeader ? std::round(24.0f * scale) : std::round(20.0f * scale);
        const float valueSlotW = hasHeader ? 0.0f : std::round(50.0f * scale);
        const float trackW = std::max(40.0f, w - valueSlotW);
        bool changed = false;

        if (isEditingText)
        {
            ImGui::SetNextItemWidth(w);
            if (ImGui::InputFloat("##scalar_edit", value, 0.0f, 0.0f, fmt, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                *value = std::clamp(*value, std::min(minVal, maxVal), std::max(minVal, maxVal));
                storage->SetBool(editStateId, false);
                changed = true;
            }
            if (!ImGui::IsItemActive() && !ImGui::IsItemFocused() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())
            {
                *value = std::clamp(*value, std::min(minVal, maxVal), std::max(minVal, maxVal));
                storage->SetBool(editStateId, false);
            }
            ImGui::PopID();
            return changed;
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##track", ImVec2(w, interactH));
        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();

        if (isHovered && (ImGui::GetIO().KeyCtrl || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)))
        {
            storage->SetBool(editStateId, true);
            ImGui::SetKeyboardFocusHere(-1);
        }

        const float span = maxVal - minVal;
        if (isActive && std::abs(span) > 1e-6f)
        {
            const float thumbW = std::round(m.sliderThumbWidth * scale);
            const float usableW = std::max(1.0f, trackW - thumbW);
            const float relX = std::clamp((ImGui::GetIO().MousePos.x - (pos.x + thumbW * 0.5f)) / usableW, 0.0f, 1.0f);
            const float nextVal = minVal + relX * span;
            if (std::abs(nextVal - *value) > 1e-6f)
            {
                *value = nextVal;
                changed = true;
            }
        }

        if (isHovered || isActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const float norm = (std::abs(span) > 1e-6f) ? std::clamp((*value - minVal) / span, 0.0f, 1.0f) : 0.0f;
        const float trackH = std::round(m.sliderTrackHeight * scale);
        const float trackY = pos.y + std::round((interactH - trackH) * 0.5f);
        const float thumbW = std::round(m.sliderThumbWidth * scale);
        const float thumbH = std::round((hasHeader ? m.sliderThumbHeight : 14.0f) * scale);
        const float thumbX = pos.x + std::round((trackW - thumbW) * norm);
        const float thumbY = pos.y + std::round((interactH - thumbH) * 0.5f);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 trackMin(pos.x, trackY);
        const ImVec2 trackMax(pos.x + trackW, trackY + trackH);
        dl->AddRectFilled(trackMin, trackMax, ToU32(t.secondaryHover), 0.0f);

        const ImVec2 fillMax(thumbX + thumbW * 0.5f, trackMax.y);
        dl->AddRectFilled(trackMin, fillMax, ToU32(t.primary), 0.0f);

        const ImVec2 thumbMin(thumbX, thumbY);
        const ImVec2 thumbMax(thumbX + thumbW, thumbY + thumbH);
        if (isHovered || isActive)
        {
            dl->AddRect(
                ImVec2(thumbMin.x - 2.0f, thumbMin.y - 2.0f),
                ImVec2(thumbMax.x + 2.0f, thumbMax.y + 2.0f),
                ToU32(t.ring, isActive ? 0.40f : 0.22f),
                0.0f,
                0,
                2.0f);
        }
        dl->AddRectFilled(thumbMin, thumbMax, ToU32(t.background), 0.0f);
        dl->AddRect(thumbMin, thumbMax, ToU32(isActive ? t.primaryHover : t.primary), 0.0f, 0, 2.0f);

        if (!hasHeader)
        {
            ScopedFont mono(FontVariant::Mono);
            const ImVec2 vs = ImGui::CalcTextSize(valBuf);
            dl->AddText(ImVec2(pos.x + w - vs.x, pos.y + std::round((interactH - vs.y) * 0.5f)), ToU32(isActive ? t.foreground : t.mutedForeground), valBuf);
        }

        ImGui::PopID();
        return changed;
    }

    bool SliderInt(const char *label, int *value, int minVal, int maxVal, const char *format, float width)
    {
        if (!value)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float w = (width > 0.0f) ? width : std::max(80.0f, ImGui::GetContentRegionAvail().x);

        const char *safeLabel = label ? label : "##slider_int";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const char *fmt = format ? format : "%d";

        char valBuf[64]{};
        std::snprintf(valBuf, sizeof(valBuf), fmt, *value);

        ImGui::PushID(safeLabel);
        const bool hasHeader = DrawSliderFloatHeader(safeLabel, labelEnd, valBuf, w, scale);

        const float interactH = hasHeader ? std::round(24.0f * scale) : std::round(20.0f * scale);
        const float valueSlotW = hasHeader ? 0.0f : std::round(44.0f * scale);
        const float trackW = std::max(40.0f, w - valueSlotW);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##track_int", ImVec2(w, interactH));
        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();

        bool changed = false;
        const int lo = std::min(minVal, maxVal);
        const int hi = std::max(minVal, maxVal);
        const int span = hi - lo;

        if (isActive && span > 0)
        {
            const float thumbW = std::round(m.sliderThumbWidth * scale);
            const float usableW = std::max(1.0f, trackW - thumbW);
            const float relX = std::clamp((ImGui::GetIO().MousePos.x - (pos.x + thumbW * 0.5f)) / usableW, 0.0f, 1.0f);
            const int nextVal = lo + static_cast<int>(std::round(relX * static_cast<float>(span)));
            if (nextVal != *value)
            {
                *value = std::clamp(nextVal, lo, hi);
                changed = true;
            }
        }

        if (isHovered || isActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const float norm = (span > 0) ? std::clamp(static_cast<float>(*value - lo) / static_cast<float>(span), 0.0f, 1.0f) : 0.0f;
        const float trackH = std::round(m.sliderTrackHeight * scale);
        const float trackY = pos.y + std::round((interactH - trackH) * 0.5f);
        const float thumbW = std::round(m.sliderThumbWidth * scale);
        const float thumbH = std::round((hasHeader ? m.sliderThumbHeight : 14.0f) * scale);
        const float thumbX = pos.x + std::round((trackW - thumbW) * norm);
        const float thumbY = pos.y + std::round((interactH - thumbH) * 0.5f);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 trackMin(pos.x, trackY);
        const ImVec2 trackMax(pos.x + trackW, trackY + trackH);
        dl->AddRectFilled(trackMin, trackMax, ToU32(t.secondaryHover), 0.0f);
        dl->AddRectFilled(trackMin, ImVec2(thumbX + thumbW * 0.5f, trackMax.y), ToU32(t.primary), 0.0f);

        const ImVec2 thumbMin(thumbX, thumbY);
        const ImVec2 thumbMax(thumbX + thumbW, thumbY + thumbH);
        if (isHovered || isActive)
        {
            dl->AddRect(
                ImVec2(thumbMin.x - 2.0f, thumbMin.y - 2.0f),
                ImVec2(thumbMax.x + 2.0f, thumbMax.y + 2.0f),
                ToU32(t.ring, isActive ? 0.40f : 0.22f),
                0.0f,
                0,
                2.0f);
        }
        dl->AddRectFilled(thumbMin, thumbMax, ToU32(t.background), 0.0f);
        dl->AddRect(thumbMin, thumbMax, ToU32(isActive ? t.primaryHover : t.primary), 0.0f, 0, 2.0f);

        if (!hasHeader)
        {
            ScopedFont mono(FontVariant::Mono);
            const ImVec2 vs = ImGui::CalcTextSize(valBuf);
            dl->AddText(ImVec2(pos.x + w - vs.x, pos.y + std::round((interactH - vs.y) * 0.5f)), ToU32(isActive ? t.foreground : t.mutedForeground), valBuf);
        }

        ImGui::PopID();
        return changed;
    }

    bool RangeSliderFloat(const char *label, float *minVal, float *maxVal, float rangeMin, float rangeMax, const char *format, float width)
    {
        if (!minVal || !maxVal)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float w = (width > 0.0f) ? width : std::max(80.0f, ImGui::GetContentRegionAvail().x);

        const char *safeLabel = label ? label : "##range_slider";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const char *fmt = format ? format : "%.1f";

        char loBuf[32]{};
        char hiBuf[32]{};
        char rangeBuf[72]{};
        std::snprintf(loBuf, sizeof(loBuf), fmt, *minVal);
        std::snprintf(hiBuf, sizeof(hiBuf), fmt, *maxVal);
        std::snprintf(rangeBuf, sizeof(rangeBuf), "%s - %s", loBuf, hiBuf);

        ImGui::PushID(safeLabel);
        DrawSliderFloatHeader(safeLabel, labelEnd, rangeBuf, w, scale);

        const float interactH = std::round(24.0f * scale);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float thumbW = std::round(m.sliderThumbWidth * scale);
        const float thumbH = std::round(m.sliderThumbHeight * scale);
        const float usableW = std::max(1.0f, w - thumbW);
        const float span = std::max(1e-5f, rangeMax - rangeMin);

        ImGui::InvisibleButton("##range_track", ImVec2(w, interactH));
        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();

        const ImGuiID activeThumbId = ImGui::GetID("##active_thumb");
        ImGuiStorage *storage = ImGui::GetStateStorage();
        int activeThumb = storage->GetInt(activeThumbId, -1);

        bool changed = false;
        const float mouseRel = std::clamp((ImGui::GetIO().MousePos.x - (pos.x + thumbW * 0.5f)) / usableW, 0.0f, 1.0f);

        if (ImGui::IsItemActivated())
        {
            const float normMin = std::clamp((*minVal - rangeMin) / span, 0.0f, 1.0f);
            const float normMax = std::clamp((*maxVal - rangeMin) / span, 0.0f, 1.0f);
            activeThumb = (std::abs(mouseRel - normMin) <= std::abs(mouseRel - normMax)) ? 0 : 1;
            storage->SetInt(activeThumbId, activeThumb);
        }
        else if (!isActive)
        {
            activeThumb = -1;
            storage->SetInt(activeThumbId, -1);
        }

        if (isActive && activeThumb >= 0)
        {
            const float targetVal = rangeMin + mouseRel * span;
            if (activeThumb == 0)
            {
                const float clamped = std::clamp(targetVal, rangeMin, *maxVal);
                if (std::abs(clamped - *minVal) > 1e-6f)
                {
                    *minVal = clamped;
                    changed = true;
                }
            }
            else
            {
                const float clamped = std::clamp(targetVal, *minVal, rangeMax);
                if (std::abs(clamped - *maxVal) > 1e-6f)
                {
                    *maxVal = clamped;
                    changed = true;
                }
            }
        }

        if (isHovered || isActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const float normLo = std::clamp((*minVal - rangeMin) / span, 0.0f, 1.0f);
        const float normHi = std::clamp((*maxVal - rangeMin) / span, 0.0f, 1.0f);

        const float trackH = std::round(m.sliderTrackHeight * scale);
        const float trackY = pos.y + std::round((interactH - trackH) * 0.5f);
        const float thumbY = pos.y + std::round((interactH - thumbH) * 0.5f);
        const float thumbLoX = pos.x + std::round(usableW * normLo);
        const float thumbHiX = pos.x + std::round(usableW * normHi);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(pos.x, trackY), ImVec2(pos.x + w, trackY + trackH), ToU32(t.secondaryHover), 0.0f);
        dl->AddRectFilled(ImVec2(thumbLoX + thumbW * 0.5f, trackY), ImVec2(thumbHiX + thumbW * 0.5f, trackY + trackH), ToU32(t.primary), 0.0f);

        auto drawThumb = [&](float x, bool focused)
        {
            const ImVec2 tMin(x, thumbY);
            const ImVec2 tMax(x + thumbW, thumbY + thumbH);
            if (focused)
            {
                dl->AddRect(ImVec2(tMin.x - 2.0f, tMin.y - 2.0f), ImVec2(tMax.x + 2.0f, tMax.y + 2.0f), ToU32(t.ring, 0.35f), 0.0f, 0, 2.0f);
            }
            dl->AddRectFilled(tMin, tMax, ToU32(t.background), 0.0f);
            dl->AddRect(tMin, tMax, ToU32(t.primary), 0.0f, 0, 2.0f);
        };

        drawThumb(thumbLoX, activeThumb == 0);
        drawThumb(thumbHiX, activeThumb == 1);

        ImGui::PopID();
        return changed;
    }
}

