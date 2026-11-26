// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "gui_function.hpp"

#include <imgui_internal.h>

namespace ignite
{
    EditorButtonDecorate::EditorButtonDecorate()
    {
        const auto &t = UI::GetTheme();
        fillColor = EditorWidgetColor(t.secondary, t.secondaryHover);
        outlineColor = EditorWidgetColor(t.border, t.borderStrong);
    }

    EditorButtonDecorate EditorButtonDecorate::Fill(const glm::vec4 &color)
    {
        fillColor = EditorWidgetColor(color);
        return *this;
    }

    EditorButtonDecorate EditorButtonDecorate::Fill(const glm::vec4 &normal, const glm::vec4 &hover)
    {
        fillColor = EditorWidgetColor(normal, hover);
        return *this;
    }

    EditorButtonDecorate EditorButtonDecorate::Outline(const glm::vec4 &color)
    {
        outlineColor = EditorWidgetColor(color);
        return *this;
    }

    EditorButtonDecorate EditorButtonDecorate::Outline(const glm::vec4 &normal, const glm::vec4 &hover)
    {
        outlineColor = EditorWidgetColor(normal, hover);
        return *this;
    }

    namespace UI
    {
        bool DrawButton(const EditorButton &button, const glm::vec2 &buttonSize, const glm::vec2 &padding, const Margin &margin)
        {
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImGuiWindow *currentWindow = ImGui::GetCurrentWindow();

            const ImVec2 &cursorPos = currentWindow->DC.CursorPos;
            ImVec2 startPosition = cursorPos;
            startPosition += { margin.Start().x, margin.Start().y };

            ImGui::SetCursorScreenPos({ startPosition.x, startPosition.y });

            const ImVec2 textSize = ImGui::CalcTextSize(button.text.c_str());
            ImVec2 paddedSize = ImVec2{ buttonSize.x + padding.x, buttonSize.y + padding.y };
            if (textSize.x + padding.x >= paddedSize.x)
            {
                paddedSize.x = textSize.x + padding.x * 2.0f;
            }

            ImGui::InvisibleButton(button.text.c_str(), paddedSize);
            const bool isHovered = ImGui::IsItemHovered();
            const bool isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            if (isHovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            const ImVec2 textStartPos = (paddedSize - textSize) * 0.5f;
            const glm::vec4 glmFillColor = button.decorate.fillColor.GetColor(isHovered);
            const glm::vec4 glmOutlineColor = button.decorate.outlineColor.GetColor(isHovered);

            drawList->AddRectFilled(startPosition, startPosition + paddedSize, ToU32(glmFillColor), 0.0f);
            drawList->AddRect(startPosition, startPosition + paddedSize, ToU32(glmOutlineColor), 0.0f, 0, 1.0f);
            drawList->AddText(startPosition + textStartPos, ToU32(GetTheme().foreground), button.text.c_str());

            startPosition.x += paddedSize.x;
            startPosition += { margin.End().x, margin.End().y };
            ImGui::SetCursorScreenPos({ startPosition.x, cursorPos.y });

            return isClicked;
        }

        void DrawHorizontalButtonList(const glm::vec2 &buttonSize, std::vector<EditorButton> buttons, const float &gap, const glm::vec2 &padding, const glm::vec2 &margin)
        {
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImVec2 &cursorPos = ImGui::GetCurrentWindow()->DC.CursorPos;

            glm::vec2 startPosition = { cursorPos.x, cursorPos.y };
            startPosition += margin;
            ImGui::SetCursorScreenPos({ startPosition.x, startPosition.y });

            for (auto &[text, decorate, onClick, isHovered, isClicked] : buttons)
            {
                const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                ImVec2 paddedSize = { buttonSize.x + padding.x, buttonSize.y + padding.y };
                if (textSize.x + padding.x >= paddedSize.x)
                {
                    paddedSize.x = textSize.x + padding.x * 2.0f;
                }

                const ImVec2 textStartPos = (paddedSize - textSize) * 0.5f;
                ImGui::InvisibleButton(text.c_str(), paddedSize);

                if (ImGui::IsItemHovered())
                {
                    isHovered = true;
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    isClicked = true;
                    if (onClick)
                        onClick();
                }

                const glm::vec4 glmFillColor = decorate.fillColor.GetColor(isHovered);
                const glm::vec4 glmOutlineColor = decorate.outlineColor.GetColor(isHovered);

                const ImVec2 buttonPos = { startPosition.x, startPosition.y };
                drawList->AddRectFilled(buttonPos, buttonPos + paddedSize, ToU32(glmFillColor), 0.0f);
                drawList->AddRect(buttonPos, buttonPos + paddedSize, ToU32(glmOutlineColor), 0.0f, 0, 1.0f);
                drawList->AddText(buttonPos + textStartPos, ToU32(GetTheme().foreground), text.c_str());

                startPosition.x += paddedSize.x + gap + margin.x;
                ImGui::SetCursorPosX(startPosition.x);
            }

            ImGui::SetCursorPosY(cursorPos.y);
        }
    }
}
