/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#   define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui.h>
#include <imgui_internal.h>

#include "ignite/math/math.hpp"

namespace ignite {

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
        glm::vec4 normal;
        glm::vec4 hover;

        EditorWidgetColor() = default;

        EditorWidgetColor(const glm::vec4 &color)
            : normal(color), hover(color)
        {
        }

        EditorWidgetColor(const glm::vec4 &normal, const glm::vec4 &hover)
            : normal(normal), hover(hover)
        {
        }

        const glm::vec4 &GetColor(bool isHovered = false) const
        {
            if (isHovered) return hover;
            return normal;
        }
    };

    struct EditorButtonDecorate : public EditorWidget
    {
        EditorWidgetColor fillColor;
        EditorWidgetColor outlineColor;

        EditorButtonDecorate()
        {
            fillColor = EditorWidgetColor(
                { 0.2f, 0.2f, 0.2f, 1.0f },
                { 0.43f, 0.43f, 0.43f, 1.0f }
            );

            outlineColor = EditorWidgetColor(
                { 0.6f, 0.6f, 0.6f, 1.0f },
                { 0.7f, 0.7f, 0.7f, 1.0f }
            );
        }

        EditorButtonDecorate Fill(const glm::vec4 &color)
        {
            fillColor = EditorWidgetColor(color);
            return *this;
        }

        EditorButtonDecorate Fill(const glm::vec4 &normal, const glm::vec4 &hover)
        {
            fillColor = EditorWidgetColor(normal, hover);
            return *this;
        }

        EditorButtonDecorate Outline(const glm::vec4 &color)
        {
            outlineColor = EditorWidgetColor(color);
            return *this;
        }

        EditorButtonDecorate Outline(const glm::vec4 &normal, const glm::vec4 &hover)
        {
            outlineColor = EditorWidgetColor(normal, hover);
            return *this;
        }
    };

    struct EditorButton
    {
        std::string text;
        EditorButtonDecorate decorate;
        
        // only for getter
        std::function<void()> onClick;

        bool isHovered = false;
        bool isClicked = false;

        EditorButton(const std::string &text)
            : text(text)
        {
        }

        EditorButton(const std::string &text, const EditorButtonDecorate &decorate)
            : text(text), decorate(decorate)
        {
        }

        EditorButton(const std::string &text, const EditorButtonDecorate &decorate, const std::function<void()> &onClick)
            : text(text), decorate(decorate), onClick(onClick)
        {
        }
    };

namespace UI {

    bool DrawButton(const EditorButton &button, const glm::vec2 &buttonSize, const glm::vec2 &padding, const Margin &margin)
    {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImGuiWindow *currentWindow = ImGui::GetCurrentWindow();

        // save cursor pos
        const ImVec2 &cursorPos = currentWindow->DC.CursorPos;
        ImVec2 startPosition = cursorPos; // use cursor pos (not position)

        startPosition += { margin.Start().x, margin.Start().y };

        ImGui::SetCursorScreenPos({ startPosition.x, startPosition.y });

        // Create an invisible button (captures input and sets hovered / clicked state)
        ImGui::InvisibleButton(button.text.c_str(), ImVec2{ buttonSize.x + padding.x, buttonSize.y + padding.y });

        bool isHovered = ImGui::IsItemHovered();

        // Handle click
        bool isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        // Draw the text centered
        ImVec2 textSize = ImGui::CalcTextSize(button.text.c_str());
        ImVec2 paddedSize = ImVec2 { buttonSize.x + padding.x, buttonSize.y + padding.y };
        
        if (textSize.x + padding.x >= paddedSize.x)
        {
            paddedSize.x = textSize.x * 2.0f + padding.x - paddedSize.x;
        }

        ImVec2 textStartPos = (paddedSize - textSize) * 0.5f;

        glm::vec4 glmFillColor = button.decorate.fillColor.GetColor(isHovered);
        glm::vec4 glmOutlineColor = button.decorate.outlineColor.GetColor(isHovered);

        ImColor fillColor = { glmFillColor.r, glmFillColor.g, glmFillColor.b, glmFillColor.w };
        ImColor outlineColor = { glmOutlineColor.r, glmOutlineColor.g, glmOutlineColor.b, glmOutlineColor.w };

        // Draw background rectangle
        drawList->AddRectFilled(startPosition, startPosition + paddedSize, fillColor); // fill
        drawList->AddRect(startPosition, startPosition + paddedSize, outlineColor); // outline
        drawList->AddText({ startPosition + textStartPos }, IM_COL32(255, 255, 255, 255), button.text.c_str());

        // update screen pos
        startPosition.x += paddedSize.x;
        startPosition += { margin.End().x, margin.End().y };

        ImGui::SetCursorScreenPos({ startPosition.x, cursorPos.y }); // reset y cursor pos from saved one

        return isClicked;
    }

    void DrawHorizontalButtonList(const glm::vec2 &buttonSize, std::vector<EditorButton> buttons, const float &gap, const glm::vec2 &padding, const glm::vec2 &margin)
    {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImGuiWindow *currentWindow = ImGui::GetCurrentWindow();
        const ImVec2 &cursorPos = ImGui::GetCurrentWindow()->DC.CursorPos;

        glm::vec2 startPosition = { cursorPos.x, cursorPos.y }; // use cursor pos (not position)
        startPosition += margin;

        // update screen pos
        ImGui::SetCursorScreenPos({ startPosition.x, startPosition.y });

        for (auto &[text, decorate, onClick, isHovered, isClicked] : buttons)
        {
            ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
            ImVec2 paddedSize = { buttonSize.x + padding.x, buttonSize.y + padding.y };

            if (textSize.x + padding.x >= paddedSize.x)
            {
                paddedSize.x = textSize.x * 2.0f + padding.x - paddedSize.x;
            }

            // Draw the text centered
            ImVec2 textStartPos = (paddedSize - textSize) * 0.5f;

            // Create an invisible button (captures input and sets hovered / clicked state)
            ImGui::InvisibleButton(text.c_str(), paddedSize);

            if (ImGui::IsItemHovered())
                isHovered = true;

            // Handle click
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                isClicked = true;
                onClick();
            }

            glm::vec4 glmFillColor = decorate.fillColor.GetColor(isHovered);
            glm::vec4 glmOutlineColor = decorate.outlineColor.GetColor(isHovered);

            ImColor fillColor = { glmFillColor.r, glmFillColor.g, glmFillColor.b, glmFillColor.w };
            ImColor outlineColor = { glmOutlineColor.r, glmOutlineColor.g, glmOutlineColor.b, glmOutlineColor.w };

            // Draw background rectangle
            const ImVec2 buttonPos = { startPosition.x, startPosition.y };
            drawList->AddRectFilled(buttonPos, buttonPos + paddedSize, fillColor); // fill
            drawList->AddRect(buttonPos, buttonPos + paddedSize, outlineColor); // outline
            drawList->AddText({ buttonPos + textStartPos }, IM_COL32(255, 255, 255, 255), text.c_str());

            // increment Position
            startPosition.x += buttonSize.x + gap;
            startPosition.x += padding.x; // only add x (Horizontally growing)
            startPosition.x += margin.x; // only add x (Horizontally growing)

            // update screen pos
            ImGui::SetCursorPosX(startPosition.x); // reset Y pos
        }

        ImGui::SetCursorPosY(cursorPos.y); // reset Y pos
    }
} // UI namespace

}
