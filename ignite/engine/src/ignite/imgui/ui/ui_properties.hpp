// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_PROPERTIES_HPP
#define IGN_UI_PROPERTIES_HPP

#include "ui_theme.hpp"
#include "ignite/graphics/texture.hpp"

#include <climits>
#include <functional>
#include <string>

namespace ignite::UI
{
    class IGN_API ScopedCompactInspector
    {
    public:
        ScopedCompactInspector();
        ~ScopedCompactInspector();

        ScopedCompactInspector(const ScopedCompactInspector &) = delete;
        ScopedCompactInspector &operator=(const ScopedCompactInspector &) = delete;
    };
    inline ImTextureID GetTextureID(const Ref<Texture> &texture)
    {
        return (texture && texture->GetShaderResourceView())
            ? reinterpret_cast<ImTextureID>(texture->GetShaderResourceView())
            : 0;
    }

    inline ImTextureID GetTextureID(Texture *texture)
    {
        return (texture && texture->GetShaderResourceView())
            ? reinterpret_cast<ImTextureID>(texture->GetShaderResourceView())
            : 0;
    }

    struct IGN_API State
    {
        bool isItemActivated = false;
        bool isItemEdited = false;
        bool isItemDeactivatedAfterEdit = false;

        operator bool() const
        {
            return isItemActivated || isItemEdited || isItemDeactivatedAfterEdit;
        }

        static void Check(State &s);
    };

    extern IGN_API float defColWidth;

    IGN_API State DrawButtonWithColumn(
        const char *label,
        const char *text,
        bool *value = nullptr,
        std::function<void()> func = {},
        float columnWidth = defColWidth);

    IGN_API State DrawCheckbox(const char *label, bool *value, float columnWidth = defColWidth);
    IGN_API State DrawCheckbox2(const char *label, bool *x, bool *y, float columnWidth = defColWidth);
    IGN_API State DrawCheckbox3(const char *label, bool *x, bool *y, bool *z, float columnWidth = defColWidth);
    IGN_API State DrawSwitch(const char *label, bool *value, float columnWidth = defColWidth);

    IGN_API State DrawColorVec3(const char *label, glm::vec3 &v, float columnWidth = defColWidth);
    IGN_API State DrawColorVec4(const char *label, glm::vec4 &v, float columnWidth = defColWidth);

    IGN_API State DrawComboBox(
        const char *label,
        const char *const *labels,
        int labelsCount,
        int *selectedIndex,
        float columnWidth = defColWidth);

    IGN_API State DrawTextControl(
        const char *label,
        std::string &value,
        const char *placeholder = "",
        std::function<void()> afterFieldFunc = {},
        float columnWidth = defColWidth);

    IGN_API State DrawVec2Control(const char *label, glm::vec2 &values, float speed = 0.025f, float resetValue = 0.0f, float columnWidth = defColWidth);
    IGN_API State DrawVec3Control(const char *label, glm::vec3 &values, float speed = 0.025f, float resetValue = 0.0f, float columnWidth = defColWidth);
    IGN_API State DrawVec4Control(const char *label, glm::vec4 &values, float speed = 0.025f, float resetValue = 0.0f, float columnWidth = defColWidth);

    IGN_API State DrawFloatControl(const char *label, float *value, float speed = 0.025f, float minValue = 0.0f, float maxValue = 1.0f, float resetValue = 0.0f, float columnWidth = defColWidth);
    IGN_API State DrawIntControl(const char *label, int *value, float speed = 1.0f, int minValue = 0, int maxValue = INT_MAX, int resetValue = 0, float columnWidth = defColWidth);

    IGN_API State DrawSliderFloat(const char *label, float *value, float minValue = 0.0f, float maxValue = 1.0f, float columnWidth = defColWidth);
    IGN_API State DrawSliderInt(const char *label, int *value, int minValue = 0, int maxValue = 100, float columnWidth = defColWidth);
}

#endif

