// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GUI_FUNCTION_HPP
#define IGN_GUI_FUNCTION_HPP

#include "ignite/core/base.hpp"
#include "ignite/math/math.hpp"
#include "ignite/imgui/ui/ui.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ignite
{
    struct EditorWidget
    {
        template<typename T>
        T *As()
        {
            return static_cast<T *>(this);
        }
    };

    struct EditorWidgetColor : public EditorWidget
    {
        glm::vec4 normal{};
        glm::vec4 hover{};

        EditorWidgetColor() = default;

        EditorWidgetColor(const glm::vec4 &color)
            : normal(color), hover(color)
        { }

        EditorWidgetColor(const glm::vec4 &normal, const glm::vec4 &hover)
            : normal(normal), hover(hover)
        { }

        const glm::vec4 &GetColor(bool isHovered = false) const
        {
            if (isHovered) return hover;
            return normal;
        }
    };

    struct IGN_API EditorButtonDecorate : public EditorWidget
    {
        EditorWidgetColor fillColor;
        EditorWidgetColor outlineColor;

        EditorButtonDecorate();

        EditorButtonDecorate Fill(const glm::vec4 &color);
        EditorButtonDecorate Fill(const glm::vec4 &normal, const glm::vec4 &hover);
        EditorButtonDecorate Outline(const glm::vec4 &color);
        EditorButtonDecorate Outline(const glm::vec4 &normal, const glm::vec4 &hover);
    };

    struct EditorButton
    {
        std::string text;
        EditorButtonDecorate decorate;
        std::function<void()> onClick;

        bool isHovered = false;
        bool isClicked = false;

        EditorButton(const std::string &text)
            : text(text)
        { }

        EditorButton(const std::string &text, const EditorButtonDecorate &decorate)
            : text(text), decorate(decorate)
        { }

        EditorButton(const std::string &text, const EditorButtonDecorate &decorate, const std::function<void()> &onClick)
            : text(text), decorate(decorate), onClick(onClick)
        { }
    };

    namespace UI
    {
        IGN_API bool DrawButton(const EditorButton &button, const glm::vec2 &buttonSize, const glm::vec2 &padding, const Margin &margin);
        IGN_API void DrawHorizontalButtonList(const glm::vec2 &buttonSize, std::vector<EditorButton> buttons, const float &gap, const glm::vec2 &padding, const glm::vec2 &margin);
    }
}

#endif
