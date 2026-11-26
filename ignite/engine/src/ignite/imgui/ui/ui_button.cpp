// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_button.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ignite::UI
{
    namespace
    {
        float GetButtonHeight(ButtonSize size, float scale)
        {
            const Metrics &m = GetMetrics();
            switch (size)
            {
                case ButtonSize::Small:
                case ButtonSize::IconSm: return std::round(m.buttonSm * scale);
                case ButtonSize::Large:
                case ButtonSize::IconLg: return std::round(m.buttonLg * scale);
                case ButtonSize::Medium:
                case ButtonSize::IconMd:
                default:                 return std::round(m.buttonMd * scale);
            }
        }

        float GetButtonPadX(ButtonSize size, float scale)
        {
            switch (size)
            {
                case ButtonSize::Small:  return std::round(12.0f * scale);
                case ButtonSize::Large:  return std::round(22.0f * scale);
                case ButtonSize::IconSm:
                case ButtonSize::IconMd:
                case ButtonSize::IconLg: return 0.0f;
                case ButtonSize::Medium:
                default:                 return std::round(16.0f * scale);
            }
        }

        bool IsIconButtonSize(ButtonSize size)
        {
            return size == ButtonSize::IconSm || size == ButtonSize::IconMd || size == ButtonSize::IconLg;
        }

        void ResolveVariantColors(
            ButtonVariant variant,
            bool isHovered,
            bool isActive,
            glm::vec4 &outBg,
            glm::vec4 &outBorder,
            glm::vec4 &outText,
            bool &outDrawBorder)
        {
            const ThemeColors &t = GetTheme();
            outDrawBorder = true;

            switch (variant)
            {
                case ButtonVariant::Primary:
                    outBg = isActive ? t.primaryActive : (isHovered ? t.primaryHover : t.primary);
                    outBorder = outBg;
                    outText = t.primaryForeground;
                    outDrawBorder = false;
                    break;
                case ButtonVariant::Secondary:
                    outBg = isActive ? t.secondaryActive : (isHovered ? t.secondaryHover : t.secondary);
                    outBorder = isHovered ? t.borderStrong : t.border;
                    outText = t.foreground;
                    break;
                case ButtonVariant::Outline:
                    outBg = isActive ? t.secondaryActive : (isHovered ? t.secondary : glm::vec4(t.background.r, t.background.g, t.background.b, 0.0f));
                    outBorder = isHovered ? t.borderStrong : t.border;
                    outText = t.foreground;
                    break;
                case ButtonVariant::Ghost:
                    outBg = isActive ? t.secondaryActive : (isHovered ? t.secondaryHover : glm::vec4(0.0f));
                    outBorder = glm::vec4(0.0f);
                    outText = isHovered ? t.foreground : t.mutedForeground;
                    outDrawBorder = false;
                    break;
                case ButtonVariant::Destructive:
                    outBg = isActive ? t.destructive : (isHovered ? t.destructiveHover : t.destructive);
                    outBorder = t.destructiveBorder;
                    outText = t.destructiveForeground;
                    break;
                case ButtonVariant::Link:
                    outBg = glm::vec4(0.0f);
                    outBorder = glm::vec4(0.0f);
                    outText = isHovered ? t.foreground : t.mutedForeground;
                    outDrawBorder = false;
                    break;
            }
        }

        void DrawSpinnerArc(ImDrawList *dl, const ImVec2 &center, float radius, float thickness, ImU32 color)
        {
            const float t = static_cast<float>(ImGui::GetTime());
            const float start = t * 6.5f;
            const float span = 3.8f;
            dl->PathClear();
            dl->PathArcTo(center, radius, start, start + span, 16);
            dl->PathStroke(color, ImDrawFlags_None, thickness);
        }
    }

    bool Button(
        const char *label,
        ButtonVariant variant,
        ButtonSize size,
        const ImVec2 &customSize,
        const char *leadingIcon,
        bool loading,
        bool disabled)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const float scale = std::max(GetDpiScale(), 1.0f);
        const char *safeLabel = label ? label : "##btn";
        const char *labelEnd = ImGui::FindRenderedTextEnd(safeLabel);
        const bool hasText = (labelEnd > safeLabel);
        const bool hasIcon = (leadingIcon && leadingIcon[0] != '\0');

        const float defaultH = GetButtonHeight(size, scale);
        const float padX = GetButtonPadX(size, scale);
        const float gap = std::round(6.0f * scale);
        const float spinnerR = std::round(5.5f * scale);

        const ImVec2 textSize = hasText ? ImGui::CalcTextSize(safeLabel, labelEnd, false) : ImVec2(0.0f, ImGui::GetFontSize());
        const ImVec2 iconSize = hasIcon ? ImGui::CalcTextSize(leadingIcon) : ImVec2(0.0f, 0.0f);

        float contentW = 0.0f;
        if (loading)
        {
            contentW += spinnerR * 2.0f;
            if (hasText)
                contentW += gap;
        }
        else if (hasIcon)
        {
            contentW += iconSize.x;
            if (hasText)
                contentW += gap;
        }
        if (hasText)
            contentW += textSize.x;

        ImVec2 btnSize = customSize;
        if (btnSize.y <= 0.0f)
            btnSize.y = defaultH;
        if (btnSize.x < 0.0f)
        {
            const float offset = (btnSize.x < -1.0f) ? btnSize.x : 0.0f;
            btnSize.x = std::max(1.0f, ImGui::GetContentRegionAvail().x + offset);
        }
        else if (btnSize.x == 0.0f)
        {
            btnSize.x = IsIconButtonSize(size) ? btnSize.y : std::max(btnSize.y, contentW + padX * 2.0f);
        }

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const bool blockInput = disabled || loading;

        if (disabled)
            ImGui::BeginDisabled(true);

        const bool clicked = ImGui::InvisibleButton(safeLabel, btnSize);
        const bool isHovered = !blockInput && ImGui::IsItemHovered();
        const bool isHeld = !blockInput && ImGui::IsItemActive();
        const bool isFocused = !blockInput && ImGui::IsItemFocused();

        if (isHovered && !blockInput)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        const ImGuiID id = ImGui::GetItemID();
        const float hoverAnim = AnimateFloat(id ^ 0x41B29u, isHovered ? 1.0f : 0.0f, 18.0f);

        glm::vec4 bgIdle, borderIdle, textIdle;
        glm::vec4 bgHov, borderHov, textHov;
        bool drawBorder = true;
        ResolveVariantColors(variant, false, false, bgIdle, borderIdle, textIdle, drawBorder);
        ResolveVariantColors(variant, true, isHeld, bgHov, borderHov, textHov, drawBorder);

        const glm::vec4 bgCol = LerpColor(bgIdle, bgHov, isHeld ? 1.0f : hoverAnim);
        const glm::vec4 borderCol = LerpColor(borderIdle, borderHov, isHeld ? 1.0f : hoverAnim);
        const glm::vec4 textCol = LerpColor(textIdle, textHov, isHeld ? 1.0f : hoverAnim);

        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 pMin = pos;
        const ImVec2 pMax = ImVec2(pos.x + btnSize.x, pos.y + btnSize.y);

        if (bgCol.a > 0.001f)
            dl->AddRectFilled(pMin, pMax, ToU32(bgCol), 0.0f);

        if (drawBorder && borderCol.a > 0.001f)
            dl->AddRect(pMin, pMax, ToU32(borderCol), 0.0f, 0, 1.0f);

        if (isFocused)
        {
            const ThemeColors &t = GetTheme();
            dl->AddRect(ImVec2(pMin.x - 1.0f, pMin.y - 1.0f), ImVec2(pMax.x + 1.0f, pMax.y + 1.0f), ToU32(t.ring, 0.85f), 0.0f, 0, 1.0f);
        }

        const ImU32 fgU32 = ToU32(textCol);
        float drawX = pMin.x + std::round((btnSize.x - contentW) * 0.5f);
        if (drawX < pMin.x + 4.0f)
            drawX = pMin.x + 4.0f;

        if (loading)
        {
            const ImVec2 spinCenter(drawX + spinnerR, pMin.y + btnSize.y * 0.5f);
            DrawSpinnerArc(dl, spinCenter, spinnerR, 1.8f, fgU32);
            drawX += spinnerR * 2.0f + gap;
        }
        else if (hasIcon)
        {
            const float iconY = pMin.y + std::round((btnSize.y - iconSize.y) * 0.5f);
            dl->AddText(ImVec2(drawX, iconY), fgU32, leadingIcon);
            drawX += iconSize.x + gap;
        }

        if (hasText)
        {
            const float textY = pMin.y + std::round((btnSize.y - textSize.y) * 0.5f);
            const ImVec4 textClip(pMin.x + 4.0f, pMin.y, std::max(pMin.x + 6.0f, pMax.x - 4.0f), pMax.y);
            dl->AddText(nullptr, 0.0f, ImVec2(drawX, textY), fgU32, safeLabel, labelEnd, 0.0f, &textClip);
            if (variant == ButtonVariant::Link && isHovered)
            {
                const float underlineY = textY + textSize.y + 1.0f;
                dl->AddLine(ImVec2(drawX, underlineY), ImVec2(std::min(drawX + textSize.x, textClip.z), underlineY), fgU32, 1.0f);
            }
        }

        if (disabled)
            ImGui::EndDisabled();

        return clicked && !blockInput;
    }

    bool IconButton(
        const char *id,
        const char *iconGlyph,
        ButtonVariant variant,
        ButtonSize size,
        const char *tooltip,
        bool active,
        bool disabled)
    {
        ImGui::PushID(id);
        const ButtonVariant effectiveVariant = active ? ButtonVariant::Primary : variant;
        const bool clicked = Button("##icon_btn", effectiveVariant, size, ImVec2(0.0f, 0.0f), iconGlyph, false, disabled);
        if (tooltip && tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::PopID();
        return clicked;
    }

    bool ToggleButton(
        const char *label,
        bool *active,
        ButtonSize size,
        const ImVec2 &customSize,
        const char *leadingIcon)
    {
        const bool isOn = active && *active;
        const ButtonVariant variant = isOn ? ButtonVariant::Primary : ButtonVariant::Outline;
        const bool clicked = Button(label, variant, size, customSize, leadingIcon, false, false);
        if (clicked && active)
        {
            *active = !(*active);
        }
        return clicked;
    }

    bool ButtonGroup(
        const char *id,
        std::span<const char *const> labels,
        int *selectedIndex,
        ButtonSize size,
        float fullWidth)
    {
        if (labels.empty() || !selectedIndex)
            return false;

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float containerPad = std::round(3.0f * scale);
        const float itemH = GetButtonHeight(size, scale);
        const float totalH = itemH + containerPad * 2.0f;
        const float padX = std::round(12.0f * scale);

        std::vector<float> itemWidths(labels.size(), 0.0f);
        float sumWidths = 0.0f;
        for (size_t i = 0; i < labels.size(); ++i)
        {
            const char *lbl = labels[i] ? labels[i] : "";
            const ImVec2 ts = ImGui::CalcTextSize(lbl);
            itemWidths[i] = ts.x + padX * 2.0f;
            sumWidths += itemWidths[i];
        }

        float totalW = sumWidths + containerPad * 2.0f;
        if (fullWidth > 0.0f)
        {
            totalW = fullWidth;
            const float each = std::max(20.0f, (fullWidth - containerPad * 2.0f) / static_cast<float>(labels.size()));
            std::fill(itemWidths.begin(), itemWidths.end(), each);
        }

        ImGui::PushID(id);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList *dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(origin, ImVec2(origin.x + totalW, origin.y + totalH), ToU32(t.inputBg), 0.0f);
        dl->AddRect(origin, ImVec2(origin.x + totalW, origin.y + totalH), ToU32(t.border), 0.0f, 0, 1.0f);

        bool changed = false;
        float curX = origin.x + containerPad;
        const float curY = origin.y + containerPad;

        for (int i = 0; i < static_cast<int>(labels.size()); ++i)
        {
            const float w = itemWidths[static_cast<size_t>(i)];
            const ImVec2 itemMin(curX, curY);
            const ImVec2 itemMax(curX + w, curY + itemH);

            ImGui::SetCursorScreenPos(itemMin);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##seg", ImVec2(w, itemH)))
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

            const bool isSelected = (*selectedIndex == i);
            if (isSelected)
            {
                dl->AddRectFilled(itemMin, itemMax, ToU32(t.secondaryHover), 0.0f);
                dl->AddRect(itemMin, itemMax, ToU32(t.borderStrong), 0.0f, 0, 1.0f);
            }
            else if (hovered)
            {
                dl->AddRectFilled(itemMin, itemMax, ToU32(t.secondary), 0.0f);
            }

            const char *lbl = labels[static_cast<size_t>(i)] ? labels[static_cast<size_t>(i)] : "";
            const ImVec2 ts = ImGui::CalcTextSize(lbl);
            const ImVec2 textPos(itemMin.x + std::round((w - ts.x) * 0.5f), itemMin.y + std::round((itemH - ts.y) * 0.5f));
            dl->AddText(textPos, ToU32(isSelected ? t.foreground : t.mutedForeground), lbl);

            ImGui::PopID();
            curX += w;
        }

        // Advance cursor cleanly past the entire container
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy(ImVec2(totalW, totalH));
        ImGui::PopID();
        return changed;
    }

    bool DrawButton(const char *label, const ImVec2 &size)
    {
        return Button(label, ButtonVariant::Secondary, ButtonSize::Medium, size);
    }

    bool DrawImageButton(const char *strId, ImTextureID texID, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        ImDrawList *dl = ImGui::GetWindowDrawList();

        ImGui::PushID(strId ? strId : "##img_btn");
        const ImVec2 buttonMin = ImGui::GetCursorScreenPos();
        const ImVec2 buttonMax(buttonMin.x + size.x, buttonMin.y + size.y);

        const bool clicked = ImGui::InvisibleButton("##img_btn", size);
        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();

        if (isHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (isActive)
        {
            dl->AddRectFilled(buttonMin, buttonMax, ToU32(t.secondaryActive), 0.0f);
            dl->AddRect(buttonMin, buttonMax, ToU32(t.ring), 0.0f, 0, 1.0f);
        }
        else if (isHovered)
        {
            dl->AddRectFilled(buttonMin, buttonMax, ToU32(t.secondaryHover), 0.0f);
            dl->AddRect(buttonMin, buttonMax, ToU32(t.borderStrong), 0.0f, 0, 1.0f);
        }

        if (texID)
        {
            const float pad = std::round(std::min(size.x, size.y) * 0.12f);
            dl->AddImage(texID, ImVec2(buttonMin.x + pad, buttonMin.y + pad), ImVec2(buttonMax.x - pad, buttonMax.y - pad), uv0, uv1);
        }

        ImGui::PopID();
        return clicked;
    }

    bool DrawSelectImageButton(const char *strId, ImTextureID texID, const ImVec2 &size, bool active, const ImVec2 &uv0, const ImVec2 &uv1)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        const ThemeColors &t = GetTheme();
        ImDrawList *dl = ImGui::GetWindowDrawList();

        ImGui::PushID(strId ? strId : "##sel_img_btn");
        const ImVec2 buttonMin = ImGui::GetCursorScreenPos();
        const ImVec2 buttonMax(buttonMin.x + size.x, buttonMin.y + size.y);

        const bool clicked = ImGui::InvisibleButton("##sel_img_btn", size);
        const bool isHovered = ImGui::IsItemHovered();
        const bool isHeld = ImGui::IsItemActive();

        if (isHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (active || isHeld)
        {
            dl->AddRectFilled(buttonMin, buttonMax, ToU32(t.secondaryHover), 0.0f);
            dl->AddRect(buttonMin, buttonMax, ToU32(t.primary), 0.0f, 0, 1.0f);
            dl->AddRectFilled(buttonMin, ImVec2(buttonMax.x, buttonMin.y + 2.0f), ToU32(t.primary), 0.0f);
        }
        else if (isHovered)
        {
            dl->AddRectFilled(buttonMin, buttonMax, ToU32(t.secondary), 0.0f);
            dl->AddRect(buttonMin, buttonMax, ToU32(t.borderStrong), 0.0f, 0, 1.0f);
        }

        if (texID)
        {
            const float pad = std::round(std::min(size.x, size.y) * 0.12f);
            dl->AddImage(texID, ImVec2(buttonMin.x + pad, buttonMin.y + pad), ImVec2(buttonMax.x - pad, buttonMax.y - pad), uv0, uv1);
        }

        ImGui::PopID();
        return clicked;
    }
}

