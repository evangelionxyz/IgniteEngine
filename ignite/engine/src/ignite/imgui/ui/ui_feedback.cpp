// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_feedback.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ignite::UI
{
    void LoadingSpinner(const char *id, float radius, float thickness, ImU32 color)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float r = radius * scale;
        const float thick = thickness * scale;
        const float diameter = r * 2.0f + thick * 2.0f;

        ImGui::PushID(id ? id : "##spinner");
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(diameter, diameter));

        const ImVec2 center(pos.x + diameter * 0.5f, pos.y + diameter * 0.5f);
        ImDrawList *dl = ImGui::GetWindowDrawList();

        dl->AddCircle(center, r, ToU32(t.secondaryHover), 24, thick);

        const float time = static_cast<float>(ImGui::GetTime());
        const float baseAngle = time * 4.2f;
        const float arcLen = 2.2f + 1.4f * std::sin(time * 2.6f);
        const ImU32 fg = (color != 0) ? color : ToU32(t.primary);

        dl->PathClear();
        dl->PathArcTo(center, r, baseAngle, baseAngle + arcLen, 20);
        dl->PathStroke(fg, ImDrawFlags_None, thick);

        ImGui::PopID();
    }

    void LoadingSpinnerTicks(const char *id, float radius, int spokeCount)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float r = radius * scale;
        const float diameter = r * 2.0f + 4.0f * scale;
        const int count = std::max(spokeCount, 4);

        ImGui::PushID(id ? id : "##spinner_ticks");
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(diameter, diameter));

        const ImVec2 center(pos.x + diameter * 0.5f, pos.y + diameter * 0.5f);
        ImDrawList *dl = ImGui::GetWindowDrawList();

        const float time = static_cast<float>(ImGui::GetTime());
        const float innerR = r * 0.48f;
        const float outerR = r;
        constexpr float kTwoPi = 6.283185307f;
        const int activeIdx = static_cast<int>(std::fmod(time * static_cast<float>(count) * 1.15f, static_cast<float>(count)));

        for (int i = 0; i < count; ++i)
        {
            const float angle = (static_cast<float>(i) / static_cast<float>(count)) * kTwoPi;
            const int dist = (activeIdx - i + count) % count;
            const float fade = 1.0f - (static_cast<float>(dist) / static_cast<float>(count));
            const float alpha = 0.16f + 0.84f * (fade * fade);

            const ImVec2 p0(center.x + std::cos(angle) * innerR, center.y + std::sin(angle) * innerR);
            const ImVec2 p1(center.x + std::cos(angle) * outerR, center.y + std::sin(angle) * outerR);
            dl->AddLine(p0, p1, ToU32(t.primary, alpha), 1.75f * scale);
        }

        ImGui::PopID();
    }

    void ProgressBar(float fraction, const ImVec2 &size, const char *overlayLabel, bool indeterminate)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);
        const float w = (size.x > 0.0f) ? size.x : std::max(40.0f, ImGui::GetContentRegionAvail().x);
        const float h = (size.y > 0.0f) ? size.y : std::round(8.0f * scale);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 pMax(pos.x + w, pos.y + h);
        ImGui::Dummy(ImVec2(w, h));

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, pMax, ToU32(t.secondaryHover), 0.0f);
        dl->AddRect(pos, pMax, ToU32(t.border), 0.0f, 0, 1.0f);

        if (indeterminate)
        {
            const float time = static_cast<float>(ImGui::GetTime());
            const float segW = std::max(24.0f, w * 0.32f);
            const float phase = std::fmod(time * 0.85f, 1.0f);
            const float segX = pos.x + (w + segW) * phase - segW;
            const float clipX0 = std::max(pos.x, segX);
            const float clipX1 = std::min(pMax.x, segX + segW);
            if (clipX1 > clipX0)
            {
                PushShaderEffect(dl, ShaderEffect::Shimmer, 1.25f, 16.0f);
                dl->AddRectFilled(ImVec2(clipX0, pos.y), ImVec2(clipX1, pMax.y), ToU32(t.primary), 0.0f);
                PopShaderEffect(dl);
            }
        }
        else
        {
            const float clamped = std::clamp(fraction, 0.0f, 1.0f);
            if (clamped > 0.0f)
            {
                dl->AddRectFilled(pos, ImVec2(pos.x + std::round(w * clamped), pMax.y), ToU32(t.primary), 0.0f);
            }
        }

        if (overlayLabel && overlayLabel[0] != '\0' && h >= 14.0f)
        {
            ScopedFont smallFont(FontVariant::Tiny);
            const ImVec2 ts = ImGui::CalcTextSize(overlayLabel);
            const ImVec2 tp(pos.x + std::round((w - ts.x) * 0.5f), pos.y + std::round((h - ts.y) * 0.5f));
            dl->AddText(tp, ToU32(t.mutedForeground), overlayLabel);
        }
    }

    void SkeletonBox(const ImVec2 &size)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return;

        const ThemeColors &t = GetTheme();
        const float w = (size.x > 0.0f) ? size.x : std::max(24.0f, ImGui::GetContentRegionAvail().x);
        const float h = (size.y > 0.0f) ? size.y : 16.0f;

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 pMax(pos.x + w, pos.y + h);
        ImGui::Dummy(ImVec2(w, h));

        ImDrawList *dl = ImGui::GetWindowDrawList();
        PushShaderEffect(dl, ShaderEffect::Shimmer, 1.0f, 16.0f);
        dl->AddRectFilled(pos, pMax, ToU32(t.secondary), 0.0f);
        PopShaderEffect(dl);
        dl->AddRect(pos, pMax, ToU32(t.borderSubtle), 0.0f, 0, 1.0f);
    }

    void Badge(const char *text, ButtonVariant variant, const char *leadingIcon)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return;

        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);

        const char *safeText = text ? text : "";
        const bool hasIcon = (leadingIcon && leadingIcon[0] != '\0');
        const float padX = std::round(8.0f * scale);
        const float padY = std::round(3.0f * scale);
        const float gap = hasIcon ? std::round(4.0f * scale) : 0.0f;

        ImVec2 textSz(0.0f, 0.0f);
        ImVec2 iconSz(0.0f, 0.0f);
        {
            ScopedFont smallFont(FontVariant::Small);
            textSz = ImGui::CalcTextSize(safeText);
            iconSz = hasIcon ? ImGui::CalcTextSize(leadingIcon) : ImVec2(0.0f, 0.0f);
        }

        const float w = padX * 2.0f + iconSz.x + gap + textSz.x;
        const float h = padY * 2.0f + std::max(textSz.y, iconSz.y);

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 pMax(pos.x + w, pos.y + h);
        ImGui::Dummy(ImVec2(w, h));

        glm::vec4 bg = t.secondary;
        glm::vec4 border = t.border;
        glm::vec4 fg = t.foreground;
        bool drawBorder = true;

        switch (variant)
        {
            case ButtonVariant::Primary:
                bg = t.primary;
                fg = t.primaryForeground;
                drawBorder = false;
                break;
            case ButtonVariant::Outline:
                bg = t.background;
                border = t.border;
                fg = t.mutedForeground;
                break;
            case ButtonVariant::Destructive:
                bg = t.destructive;
                border = t.destructiveBorder;
                fg = t.destructiveForeground;
                break;
            default:
                break;
        }

        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, pMax, ToU32(bg), 0.0f);
        if (drawBorder)
            dl->AddRect(pos, pMax, ToU32(border), 0.0f, 0, 1.0f);

        {
            ScopedFont smallFont(FontVariant::Small);
            float x = pos.x + padX;
            if (hasIcon)
            {
                dl->AddText(ImVec2(x, pos.y + std::round((h - iconSz.y) * 0.5f)), ToU32(fg), leadingIcon);
                x += iconSz.x + gap;
            }
            dl->AddText(ImVec2(x, pos.y + std::round((h - textSz.y) * 0.5f)), ToU32(fg), safeText);
        }
    }

    bool BeginCard(const char *id, const ImVec2 &size, bool dotGridBackdrop)
    {
        const ThemeColors &t = GetTheme();
        const float scale = std::max(GetDpiScale(), 1.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ToImVec4(t.card));
        ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(t.border));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * scale, 14.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        ImGuiChildFlags childFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
        if (size.y <= 0.0f)
            childFlags |= ImGuiChildFlags_AutoResizeY;

        const bool open = ImGui::BeginChild(id ? id : "##card", size, childFlags, ImGuiWindowFlags_None);
        if (open && dotGridBackdrop)
        {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const ImVec2 wMin = ImGui::GetWindowPos();
            const ImVec2 wSize = ImGui::GetWindowSize();
            const ImVec2 wMax(wMin.x + wSize.x, wMin.y + wSize.y);
            PushShaderEffect(dl, ShaderEffect::DotGrid, 1.0f, 18.0f);
            dl->AddRectFilled(ImVec2(wMin.x + 1.0f, wMin.y + 1.0f), ImVec2(wMax.x - 1.0f, wMax.y - 1.0f), ToU32(t.card), 0.0f);
            PopShaderEffect(dl);
        }
        return open;
    }

    void EndCard()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    void CardHeader(const char *title, const char *description)
    {
        const ThemeColors &t = GetTheme();
        if (title && title[0] != '\0')
        {
            ScopedFont semi(FontVariant::SemiBold);
            ImGui::TextUnformatted(title);
        }
        if (description && description[0] != '\0')
        {
            ScopedFont smallFont(FontVariant::Small);
            ImGui::TextColored(ToImVec4(t.mutedForeground), "%s", description);
        }
        ImGui::Spacing();
    }

    void DrawCenteredText(const char *text, uint32_t color)
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const char *safeText = text ? text : "";
        const ImVec2 textSize = ImGui::CalcTextSize(safeText);

        const ImVec2 center(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
        const ImVec2 textPos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
        dl->AddText(textPos, color, safeText);
    }

    void DrawBannerText(const char *text, const ImVec2 &size)
    {
        const ThemeColors &t = GetTheme();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const char *safeText = text ? text : "";
        const ImVec2 textSize = ImGui::CalcTextSize(safeText);
        const ImVec2 bannerMax(canvasPos.x + size.x, canvasPos.y + size.y);

        dl->AddRectFilled(canvasPos, bannerMax, ToU32(t.secondary), 0.0f);
        dl->AddRect(canvasPos, bannerMax, ToU32(t.border), 0.0f, 0, 1.0f);
        dl->AddRectFilled(canvasPos, ImVec2(bannerMax.x, canvasPos.y + 2.0f), ToU32(t.primary), 0.0f);

        const ImVec2 textPos(canvasPos.x + (size.x - textSize.x) * 0.5f, canvasPos.y + (size.y - textSize.y) * 0.5f);
        dl->AddText(textPos, ToU32(t.foreground), safeText);
    }
}

