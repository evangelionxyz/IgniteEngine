// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_input.hpp"
#include "icons_lucide.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace ignite::UI
{
    namespace
    {
        int StdStringResizeCallback(ImGuiInputTextCallbackData *data)
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                auto *str = static_cast<std::string *>(data->UserData);
                str->resize(static_cast<size_t>(data->BufTextLen));
                data->Buf = str->data();
            }
            return 0;
        }

        std::unordered_map<ImGuiID, bool> &GetPasswordRevealMap()
        {
            static std::unordered_map<ImGuiID, bool> s_Reveal;
            return s_Reveal;
        }

        bool DrawTextFieldInternal(
            const char *id,
            char *buffer,
            size_t bufferSize,
            std::string *stdString,
            const char *placeholder,
            const ImVec2 &size,
            const char *leadingIcon,
            bool clearable,
            bool isPassword,
            ImGuiInputTextFlags flags)
        {
            ImGuiWindow *window = ImGui::GetCurrentWindow();
            if (!window || window->SkipItems)
                return false;

            const ThemeColors &t = GetTheme();
            const Metrics &m = GetMetrics();
            const float scale = std::max(GetDpiScale(), 1.0f);

            const float w = (size.x > 0.0f) ? size.x : std::max(60.0f, ImGui::GetContentRegionAvail().x);
            const float h = (size.y > 0.0f) ? size.y : std::round(m.inputHeight * scale);

            const bool hasLeading = (leadingIcon && leadingIcon[0] != '\0');
            const float padX = std::round(10.0f * scale);
            const float iconW = hasLeading ? ImGui::CalcTextSize(leadingIcon).x : 0.0f;
            const float iconGap = hasLeading ? std::round(8.0f * scale) : 0.0f;
            const float trailingSlotW = (clearable || isPassword) ? std::round(24.0f * scale) : 0.0f;

            ImGui::PushID(id ? id : "##text_field");
            const ImGuiID fieldId = ImGui::GetID("##box");

            const ImVec2 boxMin = ImGui::GetCursorScreenPos();
            const ImVec2 boxMax(boxMin.x + w, boxMin.y + h);
            ImDrawList *dl = ImGui::GetWindowDrawList();

            const bool isEmpty = stdString ? stdString->empty() : (!buffer || buffer[0] == '\0');
            const bool showClear = clearable && !isEmpty;
            const bool showEye = isPassword;
            bool &revealed = GetPasswordRevealMap()[fieldId];

            const float inputX = boxMin.x + padX + iconW + iconGap;
            const float inputW = std::max(24.0f, w - (padX * 2.0f + iconW + iconGap + ((showClear || showEye) ? trailingSlotW : 0.0f)));
            const float padY = std::max(2.0f, std::round((h - ImGui::GetFontSize()) * 0.5f));

            // Draw box background first; border and focus ring are drawn after input so we know active/hovered state
            dl->AddRectFilled(boxMin, boxMax, ToU32(t.inputBg), 0.0f);

            ImGui::SetCursorScreenPos(ImVec2(inputX, boxMin.y));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, padY));

            ImGuiInputTextFlags effectiveFlags = flags;
            if (isPassword && !revealed)
                effectiveFlags |= ImGuiInputTextFlags_Password;

            ImGui::SetNextItemWidth(inputW);
            bool changed = false;
            if (stdString)
            {
                effectiveFlags |= ImGuiInputTextFlags_CallbackResize;
                if (stdString->capacity() < 64)
                    stdString->reserve(64);
                changed = ImGui::InputTextWithHint("##input", placeholder ? placeholder : "", stdString->data(), stdString->capacity() + 1, effectiveFlags, StdStringResizeCallback, stdString);
            }
            else if (buffer && bufferSize > 0)
            {
                changed = ImGui::InputTextWithHint("##input", placeholder ? placeholder : "", buffer, bufferSize, effectiveFlags);
            }

            const bool isHovered = ImGui::IsItemHovered();
            const bool isActive = ImGui::IsItemActive();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);

            if (hasLeading)
            {
                const ImVec2 iconSz = ImGui::CalcTextSize(leadingIcon);
                const float iconY = boxMin.y + std::round((h - iconSz.y) * 0.5f);
                dl->AddText(ImVec2(boxMin.x + padX, iconY), ToU32(isActive ? t.foreground : t.mutedForeground), leadingIcon);
            }

            bool trailingHovered = false;
            if (showEye || showClear)
            {
                const float btnSize = h - 4.0f;
                const ImVec2 btnMin(boxMax.x - btnSize - 2.0f, boxMin.y + 2.0f);
                const ImVec2 btnMax(btnMin.x + btnSize, btnMin.y + btnSize);
                ImGui::SetCursorScreenPos(btnMin);
                if (ImGui::InvisibleButton("##trailing", ImVec2(btnSize, btnSize)))
                {
                    if (showEye)
                    {
                        revealed = !revealed;
                    }
                    else if (showClear)
                    {
                        if (stdString)
                            stdString->clear();
                        else if (buffer && bufferSize > 0)
                            buffer[0] = '\0';
                        changed = true;
                    }
                }
                trailingHovered = ImGui::IsItemHovered();
                if (trailingHovered)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    dl->AddRectFilled(btnMin, btnMax, ToU32(t.secondaryHover), 0.0f);
                }

                const char *glyph = showEye ? (revealed ? ICON_LC_EYE_OFF : ICON_LC_EYE) : ICON_LC_X;
                const ImVec2 gs = ImGui::CalcTextSize(glyph);
                const ImVec2 gp(btnMin.x + std::round((btnSize - gs.x) * 0.5f), btnMin.y + std::round((btnSize - gs.y) * 0.5f));
                dl->AddText(gp, ToU32(trailingHovered ? t.foreground : t.mutedForeground), glyph);
            }

            // Border & shadcn focus ring
            const bool boxHovered = isHovered || trailingHovered || ImGui::IsMouseHoveringRect(boxMin, boxMax);
            if (isActive)
            {
                dl->AddRect(boxMin, boxMax, ToU32(t.primary), 0.0f, 0, 1.0f);
                dl->AddRect(ImVec2(boxMin.x - 2.0f, boxMin.y - 2.0f), ImVec2(boxMax.x + 2.0f, boxMax.y + 2.0f), ToU32(t.ring, 0.22f), 0.0f, 0, 2.0f);
            }
            else
            {
                dl->AddRect(boxMin, boxMax, ToU32(boxHovered ? t.borderStrong : t.border), 0.0f, 0, 1.0f);
            }

            // Advance cursor reliably to enclose full box
            ImGui::SetCursorScreenPos(boxMin);
            ImGui::Dummy(ImVec2(w, h));
            ImGui::PopID();
            return changed;
        }
    }

    bool TextField(
        const char *id,
        char *buffer,
        size_t bufferSize,
        const char *placeholder,
        const ImVec2 &size,
        const char *leadingIcon,
        bool clearable,
        ImGuiInputTextFlags flags)
    {
        return DrawTextFieldInternal(id, buffer, bufferSize, nullptr, placeholder, size, leadingIcon, clearable, false, flags);
    }

    bool TextField(
        const char *id,
        std::string &value,
        const char *placeholder,
        const ImVec2 &size,
        const char *leadingIcon,
        bool clearable,
        ImGuiInputTextFlags flags)
    {
        return DrawTextFieldInternal(id, nullptr, 0, &value, placeholder, size, leadingIcon, clearable, false, flags);
    }

    bool SearchField(const char *id, char *buffer, size_t bufferSize, const char *placeholder, float width)
    {
        return DrawTextFieldInternal(id, buffer, bufferSize, nullptr, placeholder, ImVec2(width, 0.0f), ICON_LC_SEARCH, true, false, ImGuiInputTextFlags_EscapeClearsAll);
    }

    bool SearchField(const char *id, std::string &value, const char *placeholder, float width)
    {
        return DrawTextFieldInternal(id, nullptr, 0, &value, placeholder, ImVec2(width, 0.0f), ICON_LC_SEARCH, true, false, ImGuiInputTextFlags_EscapeClearsAll);
    }

    bool PasswordField(const char *id, char *buffer, size_t bufferSize, const char *placeholder, float width)
    {
        return DrawTextFieldInternal(id, buffer, bufferSize, nullptr, placeholder, ImVec2(width, 0.0f), nullptr, false, true, 0);
    }

    bool PasswordField(const char *id, std::string &value, const char *placeholder, float width)
    {
        return DrawTextFieldInternal(id, nullptr, 0, &value, placeholder, ImVec2(width, 0.0f), nullptr, false, true, 0);
    }

    bool TextArea(const char *id, std::string &value, const char *placeholder, const ImVec2 &size, ImGuiInputTextFlags flags)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float w = (size.x > 0.0f) ? size.x : std::max(80.0f, ImGui::GetContentRegionAvail().x);
        const float h = (size.y > 0.0f) ? size.y : std::round(88.0f * scale);

        ImGui::PushID(id ? id : "##textarea");
        const ImVec2 boxMin = ImGui::GetCursorScreenPos();
        const ImVec2 boxMax(boxMin.x + w, boxMin.y + h);
        ImDrawList *dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(boxMin, boxMax, ToU32(t.inputBg), 0.0f);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f * scale, 8.0f * scale));

        if (value.capacity() < 128)
            value.reserve(128);

        const bool changed = ImGui::InputTextMultiline(
            "##multiline",
            value.data(),
            value.capacity() + 1,
            ImVec2(w, h),
            flags | ImGuiInputTextFlags_CallbackResize,
            StdStringResizeCallback,
            &value);

        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        if (value.empty() && placeholder && placeholder[0] != '\0')
        {
            dl->AddText(ImVec2(boxMin.x + 10.0f * scale, boxMin.y + 8.0f * scale), ToU32(t.disabledForeground), placeholder);
        }

        if (isActive)
        {
            dl->AddRect(boxMin, boxMax, ToU32(t.primary), 0.0f, 0, 1.0f);
            dl->AddRect(ImVec2(boxMin.x - 2.0f, boxMin.y - 2.0f), ImVec2(boxMax.x + 2.0f, boxMax.y + 2.0f), ToU32(t.ring, 0.22f), 0.0f, 0, 2.0f);
        }
        else
        {
            dl->AddRect(boxMin, boxMax, ToU32(isHovered ? t.borderStrong : t.border), 0.0f, 0, 1.0f);
        }

        ImGui::PopID();
        return changed;
    }
}

