// Copyright (c) 2026 Evangelion Manuhutu

#include "animator_editor.hpp"
#include "states.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <ranges>

namespace ignite
{
    bool AnimatorEditor::DrawAnimatorStateCombo(const char *label, const std::vector<AnimState> &states, std::string &value, bool allowAnyState)
    {
        std::string preview = value.empty() && allowAnyState ? "Any State" : value;
        if (preview.empty())
        {
            preview = states.empty() ? "No States" : states.front().name;
        }

        bool changed = false;
        if (ImGui::BeginCombo(label, preview.c_str()))
        {
            if (allowAnyState)
            {
                const bool selected = value.empty();
                if (ImGui::Selectable("Any State", selected))
                {
                    value.clear();
                    changed = true;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            for (const auto &state : states)
            {
                const bool selected = value == state.name;
                if (ImGui::Selectable(state.name.c_str(), selected))
                {
                    value = state.name;
                    changed = true;
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    bool AnimatorEditor::DrawAnimatorParamCombo(const char *label, const std::vector<AnimParam> &params, std::string &value)
    {
        const char *preview = value.empty() ? "Select Param" : value.c_str();
        bool changed = false;

        if (ImGui::BeginCombo(label, preview))
        {
            for (const auto &param : params)
            {
                const bool selected = value == param.name;
                if (ImGui::Selectable(param.name.c_str(), selected))
                {
                    value = param.name;
                    changed = true;
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    void AnimatorEditor::RenameAnimatorStateReferences(const Ref<AnimatorController> &animator, const std::string &oldName, const std::string &newName)
    {
        if (!animator || oldName.empty() || oldName == newName)
        {
            return;
        }

        if (animator->defaultState == oldName)
        {
            animator->defaultState = newName;
        }

        for (auto &transition : animator->transitions)
        {
            if (transition.fromState == oldName)
            {
                transition.fromState = newName;
            }
            if (transition.toState == oldName)
            {
                transition.toState = newName;
            }
        }
    }

    void AnimatorEditor::RemoveAnimatorStateReferences(const Ref<AnimatorController> &animator, const std::string &stateName)
    {
        if (!animator)
        {
            return;
        }

        if (animator->defaultState == stateName)
        {
            animator->defaultState = animator->states.empty() ? std::string {} : animator->states.front().name;
        }

        std::erase_if(animator->transitions, [&](const AnimTransition &transition)
        {
            return transition.fromState == stateName || transition.toState == stateName;
        });
    }

    void AnimatorEditor::RemoveAnimatorParamReferences(const Ref<AnimatorController> &animator, const std::string &paramName)
    {
        if (!animator)
        {
            return;
        }

        for (auto &transition : animator->transitions)
        {
            std::erase_if(transition.conditions, [&](const AnimCondition &condition)
            {
                return condition.paramName == paramName;
            });
        }
    }

    void AnimatorEditor::DrawAnimatorControllerLeftPanel(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui)
    {
        ImGui::Text("Animator Controller");
        ImGui::TextDisabled("%zu states | %zu transitions | %zu params", animator->states.size(), animator->transitions.size(), animator->params.size());
        ImGui::SeparatorText("Controller");

        if (DrawAnimatorStateCombo("Default State##ac_default", animator->states, animator->defaultState))
        {
            animator->SetDirtyFlag(true);
        }

        std::string skeletonLabel = animator->skeletonHandle == AssetHandle(0) ? "Drop Skeleton Here" : assetManager->GetAssetDisplayName(animator->skeletonHandle);
        ImGui::Button(skeletonLabel.c_str(), ImVec2(-1.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                    if (assetManager->GetMetaData(handle).type == AssetType::Skeleton)
                    {
                        animator->skeletonHandle = handle;
                        animator->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (animator->skeletonHandle != AssetHandle(0) && ImGui::Button("Clear Skeleton##ac_clear_skel", ImVec2(-1.0f, 0.0f)))
        {
            animator->skeletonHandle = AssetHandle(0);
            animator->SetDirtyFlag(true);
        }

        ImGui::SeparatorText("States");
        if (ImGui::Button("+ Add State##ac_state_add", ImVec2(-1.0f, 0.0f)))
        {
            AnimState state;
            state.name = std::format("State{}", animator->states.size() + 1);
            const int row = static_cast<int>(animator->states.size()) / 2;
            const int col = static_cast<int>(animator->states.size()) % 2;
            state.editorPos = glm::vec2(60.0f + 220.0f * static_cast<float>(col), 80.0f + 150.0f * static_cast<float>(row));
            animator->states.push_back(state);
            ui.selectedState = static_cast<int>(animator->states.size()) - 1;
            if (animator->defaultState.empty())
            {
                animator->defaultState = state.name;
            }
            animator->SetDirtyFlag(true);
        }

        ImGui::BeginChild("##ac_state_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        for (int i = 0; i < static_cast<int>(animator->states.size()); ++i)
        {
            const AnimState &state = animator->states[static_cast<size_t>(i)];
            std::string label = state.name.empty() ? std::format("State {}", i + 1) : state.name;
            if (state.name == animator->defaultState)
            {
                label += "  [Default]";
            }

            if (ImGui::Selectable(label.c_str(), ui.selectedState == i))
            {
                ui.selectedState = i;
            }

            ImGui::TextDisabled("%s", assetManager->GetAssetDisplayName(state.animHandle).c_str());
            ImGui::Separator();
        }
        ImGui::EndChild();
    }

    void AnimatorEditor::DrawAnimatorControllerGraph(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui)
    {
        ImGui::Text("State Graph");
        ImGui::TextDisabled("Drag nodes to arrange the controller.");
        ImGui::Separator();

        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.x = std::max(canvasSize.x, 120.0f);
        canvasSize.y = std::max(canvasSize.y, 220.0f);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 27, 33, 255), 10.0f);
        drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(58, 64, 74, 255), 10.0f, 0, 1.5f);

        constexpr float grid = 32.0f;
        for (float x = std::fmod(ui.canvasOffset.x, grid); x < canvasSize.x; x += grid)
        {
            drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), IM_COL32(44, 49, 58, 180));
        }
        for (float y = std::fmod(ui.canvasOffset.y, grid); y < canvasSize.y; y += grid)
        {
            drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), IM_COL32(44, 49, 58, 180));
        }

        ImGui::InvisibleButton("##ac_canvas", canvasSize, ImGuiButtonFlags_MouseButtonMiddle);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            ui.canvasOffset.x += ImGui::GetIO().MouseDelta.x;
            ui.canvasOffset.y += ImGui::GetIO().MouseDelta.y;
        }

        // Transition line
        const ImVec2 nodeSize(180.0f, 72.0f);
        for (size_t ti = 0; ti < animator->transitions.size(); ++ti)
        {
            const AnimTransition &transition = animator->transitions[ti];
            const AnimState *toState = animator->FindState(transition.toState);
            if (!toState)
            {
                continue;
            }

            ImVec2 start(canvasPos.x + ui.canvasOffset.x + 20.0f, canvasPos.y + ui.canvasOffset.y + 20.0f);
            if (const AnimState *fromState = animator->FindState(transition.fromState))
            {
                start = ImVec2(canvasPos.x + ui.canvasOffset.x + fromState->editorPos.x + nodeSize.x,
                    canvasPos.y + ui.canvasOffset.y + fromState->editorPos.y + nodeSize.y * 0.5f);
            }

            const ImVec2 end(canvasPos.x + ui.canvasOffset.x + toState->editorPos.x,
                canvasPos.y + ui.canvasOffset.y + toState->editorPos.y + nodeSize.y * 0.5f);
            const ImU32 color = ui.selectedTransition == static_cast<int>(ti) ? IM_COL32(255, 196, 92, 255) : IM_COL32(126, 170, 255, 220);

            drawList->AddBezierCubic(start, ImVec2(start.x + 70.0f, start.y),
                ImVec2(end.x - 70.0f, end.y), end, color,
                ui.selectedTransition == static_cast<int>(ti) ? 3.0f : 2.0f);
        }

        ImGui::PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);
        for (int i = 0; i < static_cast<int>(animator->states.size()); ++i)
        {
            AnimState &state = animator->states[static_cast<size_t>(i)];
            const ImVec2 nodePos(canvasPos.x + ui.canvasOffset.x + state.editorPos.x, canvasPos.y + ui.canvasOffset.y + state.editorPos.y);
            const ImVec2 nodeEnd(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y);


            ImGui::PushID(i);
            ImGui::SetCursorScreenPos(nodePos);
            ImGui::InvisibleButton("##ac_node", nodeSize);

            const bool selected = ui.selectedState == i;
            const bool itemHovered = ImGui::IsItemHovered();
            
            // Selecting item
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (ImGui::IsItemHovered())
                    ui.selectedState = i;
            }

            // Transition drag
            if (ui.draggingTransitionLine)
            {
                // The transition entered
                // and only set the toStateName if the current state name is not fromStateName
                // otherwise set it back to false
                if (itemHovered)
                {
                    ui.draggingTransitionEntered = true;
                    if (ui.fromStateName != state.name)
                        ui.toStateName = state.name;
                }
                else
                {
                    if (ui.draggingTransitionEntered && !ui.toStateName.empty())
                    {
                        // only set to false when it is the target
                        if (ui.toStateName == state.name)
                            ui.draggingTransitionEntered = false;
                    }

                    // clear if no transition entered
                    if (!ui.draggingTransitionEntered)
                        ui.toStateName.clear();
                }
            }
            
            // Select functionality
            if (selected)
            {
                if (!ui.draggingTransitionLine && itemHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    ui.draggingItem = true;
                }

                if (ui.draggingItem)
                {
                    state.editorPos.x += ImGui::GetIO().MouseDelta.x;
                    state.editorPos.y += ImGui::GetIO().MouseDelta.y;
                    animator->SetDirtyFlag(true);

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        ui.draggingItem = false;
                    }
                }
            }

            // Transition grabber
            {
                const float grabOffset = 16.0f;
                const ImVec2 grabPos = { nodePos.x - grabOffset, nodePos.y - grabOffset };
                const ImVec2 grabEnd = { nodeEnd.x + grabOffset, nodeEnd.y + grabOffset };
                const ImVec2 grabSize = { grabEnd.x - grabPos.x, grabEnd.y - grabPos.y };
                ImGui::SetCursorScreenPos(grabPos);
                ImGui::InvisibleButton("##transition_grab", grabSize);
                const bool isGrabHovered = ImGui::IsItemHovered();
                drawList->AddRectFilled(grabPos, grabEnd, isGrabHovered ? IM_COL32(255, 159, 0, 125) : IM_COL32(255, 159, 0, 0), 20.0f);

                // Begin drag if no dragging
                if (ui.draggingTransitionLine == false)
                {
                    if (!ui.draggingItem && !itemHovered && isGrabHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        ui.fromStateName = state.name;
                        ui.draggingTransitionLine = true;
                    }
                }
            }

            const bool isDefault = state.name == animator->defaultState;
            drawList->AddRectFilled(nodePos, nodeEnd, (selected || itemHovered) ? IM_COL32(56, 86, 132, 255) : IM_COL32(38, 46, 57, 255), 10.0f);
            drawList->AddRect(nodePos, nodeEnd, (isDefault || itemHovered) ? IM_COL32(255, 196, 92, 255) : IM_COL32(97, 114, 138, 255), 10.0f, 0, selected ? 2.5f : 1.5f);

            const std::string title = state.name.empty() ? std::format("State {}", i + 1) : state.name;
            drawList->AddText(ImVec2(nodePos.x + 12.0f, nodePos.y + 12.0f), IM_COL32(235, 239, 245, 255), title.c_str());
            drawList->AddText(ImVec2(nodePos.x + 12.0f, nodePos.y + 34.0f), IM_COL32(167, 176, 190, 255), assetManager->GetAssetDisplayName(state.animHandle).c_str());
            if (isDefault)
            {
                drawList->AddText(ImVec2(nodePos.x + 12.0f, nodePos.y + 52.0f), IM_COL32(255, 196, 92, 255), "Default");
            }

            ImGui::PopID();
        }

        // Draw dragging transition line
        if (ui.draggingTransitionLine)
        {
            ImVec2 start(canvasPos.x + ui.canvasOffset.x + 20.0f, canvasPos.y + ui.canvasOffset.y + 20.0f);
            if (const AnimState *fromState = animator->FindState(ui.fromStateName))
            {
                start = ImVec2(canvasPos.x + ui.canvasOffset.x + fromState->editorPos.x + nodeSize.x, canvasPos.y + ui.canvasOffset.y + fromState->editorPos.y + nodeSize.y * 0.5f);
            }

            const ImVec2 end = ImGui::GetMousePos();
            drawList->AddBezierCubic(start, ImVec2(start.x + 70.0f, start.y), ImVec2(end.x - 70.0f, end.y), end, IM_COL32(255, 196, 92, 255), 2.0f);

            // Cancel
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                ui.draggingTransitionLine = false;
                ui.fromStateName.clear();
                ui.toStateName.clear();
            }

            // Confirm
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                ui.draggingTransitionLine = false;
                if (!ui.toStateName.empty())
                {
                    // Only push when there is no same transition yet
                    auto it = std::ranges::find_if(animator->transitions, [&](AnimTransition &transition)
                    {
                        return transition.fromState == ui.fromStateName && transition.toState == ui.toStateName;
                    });
                    if (it == animator->transitions.end())
                    {
                        animator->transitions.push_back({ ui.fromStateName, ui.toStateName });
                    }

                    ui.fromStateName.clear();
                    ui.toStateName.clear();
                }
            }
        }

        ImGui::PopClipRect();
    }

    void AnimatorEditor::DrawAnimatorControllerInspectorTab(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui)
    {
        ImGui::SeparatorText("Selected State");
        if (ui.selectedState >= 0 && ui.selectedState < static_cast<int>(animator->states.size()))
        {
            AnimState &state = animator->states[static_cast<size_t>(ui.selectedState)];
            const std::string oldName = state.name;

            char nameBuffer[256] {};
            std::strncpy(nameBuffer, state.name.c_str(), sizeof(nameBuffer) - 1);
            if (ImGui::InputText("Name##ac_sel_name", nameBuffer, sizeof(nameBuffer)))
            {
                state.name = nameBuffer;
                RenameAnimatorStateReferences(animator, oldName, state.name);
                animator->SetDirtyFlag(true);
            }

            std::string animationLabel = state.animHandle == AssetHandle(0) ? "Drop Skeletal Animation" : assetManager->GetAssetDisplayName(state.animHandle);
            ImGui::Button(animationLabel.c_str(), ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(handle).type == AssetType::SkeletalAnimation)
                        {
                            state.animHandle = handle;
                            animator->SetDirtyFlag(true);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (state.animHandle != AssetHandle(0) && ImGui::Button("Clear Animation##ac_clear_anim", ImVec2(-1.0f, 0.0f)))
            {
                state.animHandle = AssetHandle(0);
                animator->SetDirtyFlag(true);
            }
            if (ImGui::Button("Make Default##ac_make_default", ImVec2(-1.0f, 0.0f)))
            {
                animator->defaultState = state.name;
                animator->SetDirtyFlag(true);
            }
            if (ImGui::Button("Delete State##ac_delete_state", ImVec2(-1.0f, 0.0f)))
            {
                const std::string removedState = state.name;
                animator->states.erase(animator->states.begin() + ui.selectedState);
                RemoveAnimatorStateReferences(animator, removedState);
                ui.selectedTransition = -1;
                ui.selectedState = animator->states.empty() ? -1 : std::min(ui.selectedState, static_cast<int>(animator->states.size()) - 1);
                animator->SetDirtyFlag(true);
            }
        }
        else
        {
            ImGui::TextDisabled("Select a state in the graph or state list.");
        }

        ImGui::SeparatorText("Transitions");
        if (ImGui::Button("+ Add Transition##ac_add_tr", ImVec2(-1.0f, 0.0f)))
        {
            AnimTransition transition;
            if (!animator->states.empty())
            {
                const int fromIndex = std::max(ui.selectedState, 0);
                transition.fromState = animator->states[static_cast<size_t>(std::min(fromIndex, static_cast<int>(animator->states.size()) - 1))].name;
                if (animator->states.size() > 1)
                {
                    transition.toState = animator->states[static_cast<size_t>(transition.fromState == animator->states[0].name ? 1 : 0)].name;
                }
            }
            animator->transitions.push_back(transition);
            ui.selectedTransition = static_cast<int>(animator->transitions.size()) - 1;
            animator->SetDirtyFlag(true);
        }

        ImGui::BeginChild("##ac_tr_list", ImVec2(0.0f, 160.0f), ImGuiChildFlags_Borders);
        for (int i = 0; i < static_cast<int>(animator->transitions.size()); ++i)
        {
            const AnimTransition &transition = animator->transitions[static_cast<size_t>(i)];
            const std::string label = std::format("{} -> {}",
                transition.fromState.empty() ? "Any State" : transition.fromState,
                transition.toState.empty() ? "Unset" : transition.toState);
            if (ImGui::Selectable(label.c_str(), ui.selectedTransition == i))
            {
                ui.selectedTransition = i;
            }
        }
        ImGui::EndChild();

        if (ui.selectedTransition < 0 || ui.selectedTransition >= static_cast<int>(animator->transitions.size()))
        {
            ImGui::TextDisabled("Select a transition to edit its rules.");
            return;
        }

        AnimTransition &transition = animator->transitions[static_cast<size_t>(ui.selectedTransition)];
        ImGui::SeparatorText("Transition Details");
        if (DrawAnimatorStateCombo("From##ac_tr_from", animator->states, transition.fromState, true)) animator->SetDirtyFlag(true);
        if (DrawAnimatorStateCombo("To##ac_tr_to", animator->states, transition.toState)) animator->SetDirtyFlag(true);
        if (ImGui::Checkbox("Has Exit Time##ac_tr_exit", &transition.hasExitTime)) animator->SetDirtyFlag(true);
        if (transition.hasExitTime && ImGui::DragFloat("Exit Time##ac_tr_exit_v", &transition.exitTime, 0.01f, 0.0f, 1.0f)) animator->SetDirtyFlag(true);
    }

    void AnimatorEditor::DrawAnimatorControllerConditions(const Ref<AnimatorController> &animator, AnimatorControllerEditorState &ui)
    {
        if (ui.selectedTransition < 0 || ui.selectedTransition >= static_cast<int>(animator->transitions.size()))
        {
            return;
        }

        AnimTransition &transition = animator->transitions[static_cast<size_t>(ui.selectedTransition)];
        ImGui::SeparatorText("Conditions");
        for (int ci = 0; ci < static_cast<int>(transition.conditions.size()); ++ci)
        {
            AnimCondition &condition = transition.conditions[static_cast<size_t>(ci)];
            ImGui::PushID(ci);
            if (DrawAnimatorParamCombo("Parameter##ac_cond_param", animator->params, condition.paramName)) animator->SetDirtyFlag(true);

            int opIndex = static_cast<int>(condition.op);
            if (ImGui::Combo("Operator##ac_cond_op", &opIndex, s_ConditionOpNames, IM_ARRAYSIZE(s_ConditionOpNames)))
            {
                condition.op = static_cast<AnimCondition::Op>(opIndex);
                animator->SetDirtyFlag(true);
            }

            if (const AnimParam *param = animator->GetParam(condition.paramName))
            {
                switch (param->type)
                {
                    case AnimParam::Type::Float: if (ImGui::DragFloat("Threshold##ac_cond_f", &condition.floatThreshold, 0.01f)) animator->SetDirtyFlag(true); break;
                    case AnimParam::Type::Int: if (ImGui::DragInt("Threshold##ac_cond_i", &condition.intThreshold)) animator->SetDirtyFlag(true); break;
                    case AnimParam::Type::Bool: if (ImGui::Checkbox("Threshold##ac_cond_b", &condition.boolThreshold)) animator->SetDirtyFlag(true); break;
                    case AnimParam::Type::String:
                    {
                        char buffer[256] {};
                        std::strncpy(buffer, condition.strThreshold.c_str(), sizeof(buffer) - 1);
                        if (ImGui::InputText("Threshold##ac_cond_s", buffer, sizeof(buffer)))
                        {
                            condition.strThreshold = buffer;
                            animator->SetDirtyFlag(true);
                        }
                        break;
                    }
                    default: break;
                }
            }
            else if (ImGui::DragFloat("Threshold##ac_cond_ff", &condition.floatThreshold, 0.01f))
            {
                animator->SetDirtyFlag(true);
            }

            if (ImGui::Button("Remove Condition##ac_cond_remove", ImVec2(-1.0f, 0.0f)))
            {
                transition.conditions.erase(transition.conditions.begin() + ci);
                animator->SetDirtyFlag(true);
                ImGui::PopID();
                break;
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (ImGui::Button("+ Add Condition##ac_cond_add", ImVec2(-1.0f, 0.0f)))
        {
            transition.conditions.push_back({});
            animator->SetDirtyFlag(true);
        }
        if (ImGui::Button("Delete Transition##ac_tr_delete", ImVec2(-1.0f, 0.0f)))
        {
            animator->transitions.erase(animator->transitions.begin() + ui.selectedTransition);
            ui.selectedTransition = animator->transitions.empty() ? -1 : std::min(ui.selectedTransition, static_cast<int>(animator->transitions.size()) - 1);
            animator->SetDirtyFlag(true);
        }
    }

    void AnimatorEditor::DrawAnimatorControllerParamsTab(const Ref<AnimatorController> &animator)
    {
        if (ImGui::Button("+ Float##ac_add_float")) {
            animator->params.push_back({ .name = "Speed", .type = AnimParam::Type::Float });
            animator->SetDirtyFlag(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Bool##ac_add_bool")) {
            animator->params.push_back({ .name = "IsMoving", .type = AnimParam::Type::Bool });
            animator->SetDirtyFlag(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Int##ac_add_int")) {
            animator->params.push_back({ .name = "StateIndex", .type = AnimParam::Type::Int });
            animator->SetDirtyFlag(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ String##ac_add_str")) {
            animator->params.push_back({ .name = "Tag", .type = AnimParam::Type::String });
            animator->SetDirtyFlag(true);
        }

        for (int i = 0; i < static_cast<int>(animator->params.size()); ++i)
        {
            AnimParam &param = animator->params[static_cast<size_t>(i)];
            ImGui::PushID(i);
            ImGui::Separator();

            char nameBuffer[256] {};
            std::strncpy(nameBuffer, param.name.c_str(), sizeof(nameBuffer) - 1);
            if (ImGui::InputText("Name##ac_param_name", nameBuffer, sizeof(nameBuffer)))
            {
                param.name = nameBuffer;
                animator->SetDirtyFlag(true);
            }

            int typeIndex = static_cast<int>(param.type);
            if (ImGui::Combo("Type##ac_param_type", &typeIndex, s_ParamTypeNames, IM_ARRAYSIZE(s_ParamTypeNames)))
            {
                param.type = static_cast<AnimParam::Type>(typeIndex);
                animator->SetDirtyFlag(true);
            }

            switch (param.type)
            {
                case AnimParam::Type::Float: if (ImGui::DragFloat("Value##ac_param_f", &param.floatVal, 0.01f)) animator->SetDirtyFlag(true); break;
                case AnimParam::Type::Int: if (ImGui::DragInt("Value##ac_param_i", &param.intVal)) animator->SetDirtyFlag(true); break;
                case AnimParam::Type::Bool: if (ImGui::Checkbox("Value##ac_param_b", &param.boolVal)) animator->SetDirtyFlag(true); break;
                case AnimParam::Type::String:
                {
                    char valueBuffer[256] {};
                    std::strncpy(valueBuffer, param.strVal.c_str(), sizeof(valueBuffer) - 1);
                    if (ImGui::InputText("Value##ac_param_s", valueBuffer, sizeof(valueBuffer)))
                    {
                        param.strVal = valueBuffer;
                        animator->SetDirtyFlag(true);
                    }
                    break;
                }
                default: break;
            }

            if (ImGui::Button("Delete Parameter##ac_param_delete", ImVec2(-1.0f, 0.0f)))
            {
                const std::string removedParam = param.name;
                animator->params.erase(animator->params.begin() + i);
                RemoveAnimatorParamReferences(animator, removedParam);
                animator->SetDirtyFlag(true);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }
    }
}