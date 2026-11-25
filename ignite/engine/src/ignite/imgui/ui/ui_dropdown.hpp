// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_DROPDOWN_HPP
#define IGN_UI_DROPDOWN_HPP

#include "ui_theme.hpp"
#include <span>
#include <string>

namespace ignite::UI
{
    IGN_API bool Dropdown(
        const char *id,
        const char *const *items,
        int itemsCount,
        int *selectedIndex,
        const char *placeholder = "Select...",
        float width = 0.0f,
        bool searchable = false,
        float height = 0.0f);

    IGN_API bool Dropdown(
        const char *id,
        std::span<const std::string> items,
        int *selectedIndex,
        const char *placeholder = "Select...",
        float width = 0.0f,
        bool searchable = false,
        float height = 0.0f);

    // Shadcn-styled Context Menu & Popup Menu helpers
    IGN_API bool BeginContextMenu(const char *id = nullptr);
    IGN_API bool BeginWindowContextMenu(const char *id = nullptr, ImGuiPopupFlags flags = ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems);
    IGN_API void OpenPopupMenu(const char *id);
    IGN_API bool BeginPopupMenu(const char *id);
    IGN_API void EndContextMenu();

    IGN_API bool ContextMenuItem(
        const char *label,
        const char *shortcut = nullptr,
        const char *icon = nullptr,
        bool destructive = false,
        bool enabled = true,
        const glm::vec4 &iconColor = glm::vec4(0.0f));

    IGN_API bool ContextMenuCheckboxItem(
        const char *label,
        bool *checked,
        const char *shortcut = nullptr);

    IGN_API bool ContextMenuBeginSubMenu(
        const char *label,
        const char *icon = nullptr,
        bool enabled = true,
        const glm::vec4 &iconColor = glm::vec4(0.0f));
    IGN_API void ContextMenuEndSubMenu();
    IGN_API void ContextMenuLabel(const char *text);
    IGN_API void ContextMenuSeparator();
}

#endif

