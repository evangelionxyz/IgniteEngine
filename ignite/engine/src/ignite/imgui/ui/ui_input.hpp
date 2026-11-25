// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_INPUT_HPP
#define IGN_UI_INPUT_HPP

#include "ui_theme.hpp"
#include <string>

namespace ignite::UI
{
    IGN_API bool TextField(
        const char *id,
        char *buffer,
        size_t bufferSize,
        const char *placeholder = "",
        const ImVec2 &size = ImVec2(0.0f, 28.0f),
        const char *leadingIcon = nullptr,
        bool clearable = false,
        ImGuiInputTextFlags flags = 0);

    IGN_API bool TextField(
        const char *id,
        std::string &value,
        const char *placeholder = "",
        const ImVec2 &size = ImVec2(0.0f, 28.0f),
        const char *leadingIcon = nullptr,
        bool clearable = false,
        ImGuiInputTextFlags flags = 0);

    IGN_API bool SearchField(
        const char *id,
        char *buffer,
        size_t bufferSize,
        const char *placeholder = "Search...",
        float width = 0.0f);

    IGN_API bool SearchField(
        const char *id,
        std::string &value,
        const char *placeholder = "Search...",
        float width = 0.0f);

    IGN_API bool PasswordField(
        const char *id,
        char *buffer,
        size_t bufferSize,
        const char *placeholder = "Enter password...",
        float width = 0.0f);

    IGN_API bool PasswordField(
        const char *id,
        std::string &value,
        const char *placeholder = "Enter password...",
        float width = 0.0f);

    IGN_API bool TextArea(
        const char *id,
        std::string &value,
        const char *placeholder = "",
        const ImVec2 &size = ImVec2(0.0f, 88.0f),
        ImGuiInputTextFlags flags = 0);
}

#endif

