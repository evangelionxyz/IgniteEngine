// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_BUTTON_HPP
#define IGN_UI_BUTTON_HPP

#include "ui_theme.hpp"
#include <span>

namespace ignite::UI
{
    enum class ButtonVariant : uint8_t
    {
        Primary = 0, // Solid #FAFAFA fill, #09090B dark text
        Secondary,   // #18181B fill, #27272A border, #FAFAFA text
        Outline,     // Transparent/#09090B fill, #27272A border, #FAFAFA text
        Ghost,       // Transparent idle, #18181B/#27272A on hover
        Destructive, // #7F1D1D fill, #DC2626 border, #FEF2F2 text
        Link         // Text-only with underline on hover
    };

    enum class ButtonSize : uint8_t
    {
        Small = 0, // 24px height
        Medium,    // 28px height (default)
        Large,     // 32px height
        IconSm,    // 24x24px square
        IconMd,    // 28x28px square
        IconLg     // 32x32px square
    };

    IGN_API bool Button(
        const char *label,
        ButtonVariant variant = ButtonVariant::Secondary,
        ButtonSize size = ButtonSize::Medium,
        const ImVec2 &customSize = ImVec2(0.0f, 0.0f),
        const char *leadingIcon = nullptr,
        bool loading = false,
        bool disabled = false);

    IGN_API bool IconButton(
        const char *id,
        const char *iconGlyph,
        ButtonVariant variant = ButtonVariant::Ghost,
        ButtonSize size = ButtonSize::IconMd,
        const char *tooltip = nullptr,
        bool active = false,
        bool disabled = false);

    IGN_API bool ToggleButton(
        const char *label,
        bool *active,
        ButtonSize size = ButtonSize::Medium,
        const ImVec2 &customSize = ImVec2(0.0f, 0.0f),
        const char *leadingIcon = nullptr);

    IGN_API bool ButtonGroup(
        const char *id,
        std::span<const char *const> labels,
        int *selectedIndex,
        ButtonSize size = ButtonSize::Medium,
        float fullWidth = 0.0f);

    // Legacy-compatible button helpers modernized to flat monochrome
    IGN_API bool DrawButton(const char *label, const ImVec2 &size = ImVec2(0.0f, 28.0f));
    IGN_API bool DrawImageButton(const char *strId, ImTextureID texID, const ImVec2 &size, const ImVec2 &uv0 = ImVec2(0.0f, 0.0f), const ImVec2 &uv1 = ImVec2(1.0f, 1.0f));
    IGN_API bool DrawSelectImageButton(const char *strId, ImTextureID texID, const ImVec2 &size, bool active, const ImVec2 &uv0 = ImVec2(0.0f, 0.0f), const ImVec2 &uv1 = ImVec2(1.0f, 1.0f));
}

#endif

