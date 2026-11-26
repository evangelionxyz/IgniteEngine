// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_properties.hpp"
#include "ui_button.hpp"
#include "ui_toggle.hpp"
#include "ui_slider.hpp"
#include "ui_dropdown.hpp"
#include "ui_input.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ignite::UI
{
    float defColWidth = 116.0f;

    ScopedCompactInspector::ScopedCompactInspector()
    {
        const float scale = std::max(GetDpiScale(), 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * scale, 3.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f * scale, 1.5f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f * scale, 3.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, std::round(12.0f * scale));
    }

    ScopedCompactInspector::~ScopedCompactInspector()
    {
        ImGui::PopStyleVar(4);
    }

    void State::Check(State &s)
    {
        s.isItemEdited |= ImGui::IsItemEdited();
        s.isItemActivated |= ImGui::IsItemActivated();
        s.isItemDeactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
    }

    namespace
    {
        bool BeginPropertyRow(const char *label, float columnWidth)
        {
            const float scale = std::max(GetDpiScale(), 1.0f);
            ImGui::PushID(label ? label : "##prop");
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f * scale, 1.5f * scale));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * scale, 3.0f * scale));
            if (!ImGui::BeginTable("##prop_row", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::PopStyleVar(2);
                ImGui::PopID();
                return false;
            }

            const float colW = std::round(columnWidth * scale);
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, colW);
            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();

            const char *safeLabel = label ? label : "";
            const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
            if (labelEnd > safeLabel)
            {
                const ThemeColors &t = GetTheme();
                const ImVec2 cellPos = ImGui::GetCursorScreenPos();
                const float frameH = ImGui::GetFrameHeight();
                ScopedFont smallFont(FontVariant::Small);
                const ImVec2 textSz = ImGui::CalcTextSize(safeLabel, labelEnd, false);
                const float textY = cellPos.y + std::round(std::max(0.0f, frameH - textSz.y) * 0.5f);
                const ImVec4 clipRect(cellPos.x, cellPos.y, cellPos.x + colW - 4.0f * scale, cellPos.y + std::max(frameH, textSz.y) + 2.0f);
                ImGui::GetWindowDrawList()->AddText(nullptr, 0.0f, ImVec2(cellPos.x, textY), ToU32(t.mutedForeground), safeLabel, labelEnd, 0.0f, &clipRect);
                ImGui::Dummy(ImVec2(std::min(textSz.x, colW - 4.0f * scale), frameH));
            }

            ImGui::TableNextColumn();
            return true;
        }

        void EndPropertyRow()
        {
            ImGui::EndTable();
            ImGui::PopStyleVar(2);
            ImGui::PopID();
        }

        bool DrawAxisResetButton(const char *axisLabel, const glm::vec4 &accentColor, const ImVec2 &size)
        {
            const ThemeColors &t = GetTheme();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(axisLabel, size);
            const bool hovered = ImGui::IsItemHovered();
            const bool active = ImGui::IsItemActive();

            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            ImDrawList *dl = ImGui::GetWindowDrawList();
            const ImVec2 pMax(pos.x + size.x, pos.y + size.y);
            dl->AddRectFilled(pos, pMax, ToU32(active ? t.secondaryActive : (hovered ? t.secondaryHover : t.secondary)), 0.0f);
            dl->AddRect(pos, pMax, ToU32(hovered ? t.borderStrong : t.border), 0.0f, 0, 1.0f);
            dl->AddRectFilled(pos, ImVec2(pos.x + 2.5f, pMax.y), ToU32(accentColor), 0.0f);

            const ImVec2 ts = ImGui::CalcTextSize(axisLabel);
            dl->AddText(ImVec2(pos.x + std::round((size.x - ts.x) * 0.5f), pos.y + std::round((size.y - ts.y) * 0.5f)), ToU32(t.foreground), axisLabel);
            return clicked;
        }
    }

    State DrawButtonWithColumn(const char *label, const char *text, bool *value, std::function<void()> func, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float scale = std::max(GetDpiScale(), 1.0f);
        const float h = std::round(22.0f * scale);
        const float btnW = func ? std::max(44.0f, ImGui::GetContentRegionAvail().x - std::round(28.0f * scale)) : -1.0f;
        const bool ret = Button(text ? text : "", ButtonVariant::Outline, ButtonSize::Small, ImVec2(btnW, h));
        if (ret)
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        if (func)
            func();
        State::Check(state);
        if (value)
            *value = ret;

        EndPropertyRow();
        return state;
    }

    State DrawCheckbox(const char *label, bool *value, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        if (Checkbox("##check_box", value))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawCheckbox2(const char *label, bool *x, bool *y, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        if (Checkbox("X", x))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 12.0f);
        if (Checkbox("Y", y))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawCheckbox3(const char *label, bool *x, bool *y, bool *z, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        if (Checkbox("X", x))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 12.0f);
        if (Checkbox("Y", y))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 12.0f);
        if (Checkbox("Z", z))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawSwitch(const char *label, bool *value, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        if (Switch("##switch", value))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawColorVec3(const char *label, glm::vec3 &v, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::ColorEdit3("##Color", &v.x))
            state.isItemEdited = true;
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawColorVec4(const char *label, glm::vec4 &v, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::ColorEdit4("##Color", &v.x))
            state.isItemEdited = true;
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawComboBox(const char *label, const char *const *labels, int labelsCount, int *selectedIndex, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        if (!labels || labelsCount <= 0 || !selectedIndex)
        {
            ImGui::TextDisabled("N/A");
            EndPropertyRow();
            return state;
        }

        if (*selectedIndex < 0)
            *selectedIndex = 0;
        if (*selectedIndex >= labelsCount)
            *selectedIndex = labelsCount - 1;

        const float scale = std::max(GetDpiScale(), 1.0f);
        if (Dropdown("##combo", labels, labelsCount, selectedIndex, "Select...", -1.0f, labelsCount > 8, std::round(22.0f * scale)))
        {
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawTextControl(const char *label, std::string &value, const char *placeholder, std::function<void()> afterFieldFunc, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float scale = std::max(GetDpiScale(), 1.0f);
        if (TextField("##txt", value, placeholder ? placeholder : "", ImVec2(-1.0f, std::round(22.0f * scale))))
        {
            state.isItemEdited = true;
        }
        State::Check(state);
        if (afterFieldFunc)
        {
            afterFieldFunc();
        }

        EndPropertyRow();
        return state;
    }

    State DrawVec2Control(const char *label, glm::vec2 &values, float speed, float resetValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float avail = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float fieldW = std::max(32.0f, (avail - (btnSize.x + 2.0f) * 2.0f - spacing * 3.0f) * 0.5f);

        const glm::vec4 xAccent(0.85f, 0.25f, 0.25f, 1.0f);
        const glm::vec4 yAccent(0.25f, 0.72f, 0.42f, 1.0f);

        if (DrawAxisResetButton("X", xAccent, btnSize))
        {
            values.x = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##X", &values.x, speed, 0.0f, 0.0f, "%.3f");
        State::Check(state);

        ImGui::SameLine(0.0f, spacing);
        if (DrawAxisResetButton("Y", yAccent, btnSize))
        {
            values.y = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##Y", &values.y, speed, 0.0f, 0.0f, "%.3f");
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawVec3Control(const char *label, glm::vec3 &values, float speed, float resetValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float avail = ImGui::GetContentRegionAvail().x;
        const float spacing = 4.0f;
        const float fieldW = std::max(28.0f, (avail - (btnSize.x + 2.0f) * 3.0f - spacing * 2.0f) / 3.0f);

        const glm::vec4 xAccent(0.85f, 0.25f, 0.25f, 1.0f);
        const glm::vec4 yAccent(0.25f, 0.72f, 0.42f, 1.0f);
        const glm::vec4 zAccent(0.28f, 0.55f, 0.90f, 1.0f);

        if (DrawAxisResetButton("X", xAccent, btnSize))
        {
            values.x = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##X", &values.x, speed, 0.0f, 0.0f, "%.3f");
        State::Check(state);

        ImGui::SameLine(0.0f, spacing);
        if (DrawAxisResetButton("Y", yAccent, btnSize))
        {
            values.y = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##Y", &values.y, speed, 0.0f, 0.0f, "%.3f");
        State::Check(state);

        ImGui::SameLine(0.0f, spacing);
        if (DrawAxisResetButton("Z", zAccent, btnSize))
        {
            values.z = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##Z", &values.z, speed, 0.0f, 0.0f, "%.3f");
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawVec4Control(const char *label, glm::vec4 &values, float speed, float resetValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float avail = ImGui::GetContentRegionAvail().x;
        const float spacing = 4.0f;
        const float fieldW = std::max(24.0f, (avail - (btnSize.x + 2.0f) * 4.0f - spacing * 3.0f) * 0.25f);

        const glm::vec4 xAccent(0.85f, 0.25f, 0.25f, 1.0f);
        const glm::vec4 yAccent(0.25f, 0.72f, 0.42f, 1.0f);
        const glm::vec4 zAccent(0.28f, 0.55f, 0.90f, 1.0f);
        const glm::vec4 wAccent(0.85f, 0.70f, 0.25f, 1.0f);

        auto drawComp = [&](const char *axis, const char *dragId, float &comp, const glm::vec4 &accent, bool addTrailingSpace)
        {
            if (DrawAxisResetButton(axis, accent, btnSize))
            {
                comp = resetValue;
                state.isItemEdited = true;
                state.isItemDeactivatedAfterEdit = true;
            }
            State::Check(state);
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::SetNextItemWidth(fieldW);
            ImGui::DragFloat(dragId, &comp, speed, 0.0f, 0.0f, "%.3f");
            State::Check(state);
            if (addTrailingSpace)
                ImGui::SameLine(0.0f, spacing);
        };

        drawComp("X", "##X", values.x, xAccent, true);
        drawComp("Y", "##Y", values.y, yAccent, true);
        drawComp("Z", "##Z", values.z, zAccent, true);
        drawComp("W", "##W", values.w, wAccent, false);

        EndPropertyRow();
        return state;
    }

    State DrawFloatControl(const char *label, float *value, float speed, float minValue, float maxValue, float resetValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float fieldW = std::max(40.0f, ImGui::GetContentRegionAvail().x - btnSize.x - 4.0f);

        if (DrawAxisResetButton("V", GetTheme().mutedForeground, btnSize))
        {
            *value = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragFloat("##V", value, speed, minValue, maxValue, "%.4f");
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawIntControl(const char *label, int *value, float speed, int minValue, int maxValue, int resetValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float fieldW = std::max(40.0f, ImGui::GetContentRegionAvail().x - btnSize.x - 4.0f);

        if (DrawAxisResetButton("V", GetTheme().mutedForeground, btnSize))
        {
            *value = resetValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::SetNextItemWidth(fieldW);
        ImGui::DragInt("##V", value, speed, minValue, maxValue);
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawSliderFloat(const char *label, float *value, float minValue, float maxValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float sliderW = std::max(48.0f, ImGui::GetContentRegionAvail().x - btnSize.x - 6.0f);

        if (DrawAxisResetButton("R", GetTheme().mutedForeground, btnSize))
        {
            *value = minValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 6.0f);
        if (SliderFloat("##slider_f", value, minValue, maxValue, "%.3f", sliderW))
        {
            state.isItemEdited = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }

    State DrawSliderInt(const char *label, int *value, int minValue, int maxValue, float columnWidth)
    {
        State state;
        if (!BeginPropertyRow(label, columnWidth))
            return state;

        const float lineH = ImGui::GetFrameHeight();
        const ImVec2 btnSize(lineH, lineH);
        const float sliderW = std::max(48.0f, ImGui::GetContentRegionAvail().x - btnSize.x - 6.0f);

        if (DrawAxisResetButton("R", GetTheme().mutedForeground, btnSize))
        {
            *value = minValue;
            state.isItemEdited = true;
            state.isItemDeactivatedAfterEdit = true;
        }
        State::Check(state);
        ImGui::SameLine(0.0f, 6.0f);
        if (SliderInt("##slider_i", value, minValue, maxValue, "%d", sliderW))
        {
            state.isItemEdited = true;
        }
        State::Check(state);

        EndPropertyRow();
        return state;
    }
}

