// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_TOGGLE_HPP
#define IGN_UI_TOGGLE_HPP

#include "ui_theme.hpp"
#include <span>

namespace ignite::UI
{
    IGN_API bool Switch(const char *label, bool *value, const char *description = nullptr, bool disabled = false);

    inline bool Switcher(const char *label, bool *value, const char *description = nullptr, bool disabled = false)
    {
        return Switch(label, value, description, disabled);
    }

    IGN_API bool Checkbox(const char *label, bool *value, const char *description = nullptr, bool indeterminate = false);

    IGN_API bool RadioGroup(const char *id, std::span<const char *const> options, int *selectedIndex, bool horizontal = false);
}

#endif

