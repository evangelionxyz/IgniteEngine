// Copyright (c) 2026 Evangelion Manuhutu

#include "animator_editor.hpp"
#include "states.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <ranges>

namespace ignite
{
    namespace
    {
        static std::string BuildUniqueStateName(const Ref<AnimatorController> &animator, const std::string &baseName)
        {
            std::string cleanBase = baseName.empty() ? "State" : baseName;
            auto hasStateName = [&](const std::string &name)
            {
                return std::ranges::any_of(animator->states, [&](const AnimState &state) { return state.name == name; });
            };

            if (!hasStateName(cleanBase))
            {
                return cleanBase;
            }

            int suffix = 1;
            while (hasStateName(std::format("{} {}", cleanBase, suffix)))
            {
                ++suffix;
            }

            return std::format("{} {}", cleanBase, suffix);
        }

        static void CreateAnimatorStateFromAnimation(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui,
            AssetHandle animationHandle, const ImVec2 &worldPos)
        {
            if (!animator || !assetManager || animationHandle == AssetHandle(0))
            {
                return;
            }

            const AssetMetaData &metadata = assetManager->GetMetaData(animationHandle);
            if (metadata.type != AssetType::SkeletalAnimation)
            {
                return;
            }

            AnimState state;
            state.name = BuildUniqueStateName(animator, metadata.filepath.stem().string());
            state.animHandle = animationHandle;
            state.editorPos = glm::vec2(worldPos.x, worldPos.y);
            animator->states.push_back(state);
            ui.selectedState = static_cast<int>(animator->states.size()) - 1;
            ui.selectedStates.clear();
            ui.selectedStates.insert(ui.selectedState);
            ui.anyStateSelected = false;

            if (animator->defaultState.empty())
            {
                animator->defaultState = state.name;
            }

            if (animator->skeletonHandle == AssetHandle(0))
            {
                Ref<Asset> asset = assetManager->GetAsset(animationHandle);

                if (!asset)
                {
                    asset = assetManager->GetAssetImmediate(animationHandle);
                    if (asset)
                    {
                        Ref<SkeletalAnimation> animation = asset->As<SkeletalAnimation>();
                        animator->skeletonHandle = AssetHandle(animation->GetSkeletonHandle());
                    }
                }
                else
                {
                    Ref<SkeletalAnimation> animation = asset->As<SkeletalAnimation>();
                    animator->skeletonHandle = AssetHandle(animation->GetSkeletonHandle());
                }
            }

            animator->SetDirtyFlag(true);
        }
    }

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
            state.name = BuildUniqueStateName(animator, std::format("State{}", animator->states.size() + 1));
            const int row = static_cast<int>(animator->states.size()) / 2;
            const int col = static_cast<int>(animator->states.size()) % 2;
            state.editorPos = glm::vec2(60.0f + 220.0f * static_cast<float>(col), 80.0f + 150.0f * static_cast<float>(row));
            animator->states.push_back(state);
            ui.selectedState = static_cast<int>(animator->states.size()) - 1;
            ui.selectedStates.clear();
            ui.selectedStates.insert(ui.selectedState);
            ui.anyStateSelected = false;
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
                ui.selectedStates.clear();
                ui.selectedStates.insert(i);
                ui.anyStateSelected = false;
            }

            ImGui::TextDisabled("%s", assetManager->GetAssetDisplayName(state.animHandle).c_str());
            ImGui::Separator();
        }
        ImGui::EndChild();
    }

    void AnimatorEditor::DrawAnimatorControllerGraph(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui)
    {
        ImGui::Text("State Graph");
        ImGui::TextDisabled("Middle mouse pans, wheel zooms, and dropped animations create states.");
        ImGui::Separator();

        UI::NodeGraphCanvas canvas = UI::NodeGraph::BeginCanvas("##ac_canvas", ui.graphState);
        const ImVec2 nodeSize(180.0f, 72.0f);
        const ImVec2 anyStateNodeSize(180.0f, 72.0f);
        const ImVec2 anyStatePos(ui.anyStateEditorPos.x, ui.anyStateEditorPos.y);
        const ImVec2 anyStateConnectionPos(anyStatePos.x + anyStateNodeSize.x, anyStatePos.y + anyStateNodeSize.y * 0.5f);
        const bool ctrlHeld = ImGui::GetIO().KeyCtrl;

        if (UI::NodeGraph::BeginDropTarget(canvas))
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle) && payload->IsDelivery())
                {
                    const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                    if (assetManager->GetMetaData(handle).type == AssetType::SkeletalAnimation)
                    {
                        const ImVec2 dropWorldPos = UI::NodeGraph::ToWorld(canvas, ImGui::GetIO().MousePos);
                        CreateAnimatorStateFromAnimation(animator, assetManager, ui, handle, ImVec2(dropWorldPos.x - nodeSize.x * 0.5f, dropWorldPos.y - nodeSize.y * 0.5f));
                    }
                }
            }

            UI::NodeGraph::EndDropTarget();
        }

        bool anyNodeHovered = false;
        bool anyTransitionGrabHovered = false;

        canvas.drawList->PushClipRect(canvas.screenPos, ImVec2(canvas.screenPos.x + canvas.size.x, canvas.screenPos.y + canvas.size.y), true);

        for (size_t ti = 0; ti < animator->transitions.size(); ++ti)
        {
            const AnimTransition &transition = animator->transitions[ti];
            const AnimState *toState = animator->FindState(transition.toState);
            if (!toState)
            {
                continue;
            }

            ImVec2 fromWorldPos = anyStateConnectionPos;
            if (const AnimState *fromState = animator->FindState(transition.fromState))
            {
                fromWorldPos = ImVec2(fromState->editorPos.x + nodeSize.x, fromState->editorPos.y + nodeSize.y * 0.5f);
            }

            const ImVec2 toWorldPos(toState->editorPos.x, toState->editorPos.y + nodeSize.y * 0.5f);
            const ImU32 color = ui.selectedTransition == static_cast<int>(ti) ? IM_COL32(255, 196, 92, 255) : IM_COL32(126, 170, 255, 220);
            UI::NodeGraph::DrawConnection(canvas, fromWorldPos, toWorldPos, color, ui.selectedTransition == static_cast<int>(ti) ? 3.0f : 2.0f);
        }

        {
            const UI::NodeGraphNode anyStateNode = UI::NodeGraph::BuildNode(canvas, anyStatePos, anyStateNodeSize);
            ImGui::PushID("ac_any_state_node");
            const UI::NodeInteraction anyStateInteraction = UI::NodeGraph::HandleNode(canvas, "##ac_any_state", anyStateNode);
            anyNodeHovered |= anyStateInteraction.hovered;

            if (anyStateInteraction.clicked)
            {
                if (ctrlHeld)
                {
                    ui.anyStateSelected = !ui.anyStateSelected;
                }
                else
                {
                    ui.anyStateSelected = true;
                    ui.selectedState = -1;
                    ui.selectedStates.clear();
                }
            }

            const float anyGrabOffset = 16.0f;
            const UI::NodeGraphNode anyGrabNode = UI::NodeGraph::BuildNode(canvas,
                ImVec2(anyStatePos.x + anyStateNodeSize.x - anyGrabOffset, anyStatePos.y + anyStateNodeSize.y * 0.5f - anyGrabOffset),
                ImVec2(anyGrabOffset * 2.0f, anyGrabOffset * 2.0f));
            const UI::NodeInteraction anyGrabInteraction = UI::NodeGraph::HandleNode(canvas, "##ac_any_transition_grab", anyGrabNode);
            anyTransitionGrabHovered |= anyGrabInteraction.hovered;
            canvas.drawList->AddRectFilled(anyGrabNode.screenMin, anyGrabNode.screenMax,
                anyGrabInteraction.hovered ? IM_COL32(255, 159, 0, 125) : IM_COL32(255, 159, 0, 0),
                20.0f * ui.graphState.zoom);

            if (!ui.draggingTransitionLine && !ui.draggingItem && anyGrabInteraction.clicked)
            {
                ui.fromStateName.clear();
                ui.draggingTransitionLine = true;
            }

            if (!ui.draggingTransitionLine && !ui.draggingItem && !ui.boxSelectingStates && ui.anyStateSelected && anyStateInteraction.dragging)
            {
                ui.draggingItem = true;
                ui.draggingStateIndices.assign(ui.selectedStates.begin(), ui.selectedStates.end());
            }

            canvas.drawList->AddRectFilled(anyStateNode.screenMin, anyStateNode.screenMax,
                (ui.anyStateSelected || anyStateInteraction.hovered) ? IM_COL32(64, 82, 101, 255) : IM_COL32(44, 52, 64, 255),
                10.0f * ui.graphState.zoom);
            canvas.drawList->AddRect(anyStateNode.screenMin, anyStateNode.screenMax,
                (ui.anyStateSelected || anyStateInteraction.hovered) ? IM_COL32(255, 196, 92, 255) : IM_COL32(126, 170, 255, 220),
                10.0f * ui.graphState.zoom, 0, (ui.anyStateSelected || anyStateInteraction.hovered) ? 2.5f : 1.5f);
            canvas.drawList->AddText(ImVec2(anyStateNode.screenMin.x + 12.0f * ui.graphState.zoom, anyStateNode.screenMin.y + 12.0f * ui.graphState.zoom),
                IM_COL32(235, 239, 245, 255), "Any State");
            canvas.drawList->AddText(ImVec2(anyStateNode.screenMin.x + 12.0f * ui.graphState.zoom, anyStateNode.screenMin.y + 34.0f * ui.graphState.zoom),
                IM_COL32(167, 176, 190, 255), "Transition Source");

            ImGui::PopID();
        }

        for (int i = 0; i < static_cast<int>(animator->states.size()); ++i)
        {
            AnimState &state = animator->states[static_cast<size_t>(i)];
            const UI::NodeGraphNode node = UI::NodeGraph::BuildNode(canvas, ImVec2(state.editorPos.x, state.editorPos.y), nodeSize);

            ImGui::PushID(i);
            const UI::NodeInteraction nodeInteraction = UI::NodeGraph::HandleNode(canvas, "##ac_node", node);
            const bool selected = ui.selectedStates.contains(i);
            const bool itemHovered = nodeInteraction.hovered;
            anyNodeHovered |= itemHovered;

            if (nodeInteraction.clicked)
            {
                if (ctrlHeld)
                {
                    if (selected)
                    {
                        ui.selectedStates.erase(i);
                    }
                    else
                    {
                        ui.selectedStates.insert(i);
                        ui.selectedState = i;
                    }

                    if (ui.selectedStates.empty())
                    {
                        ui.selectedState = -1;
                    }
                }
                else
                {
                    if (!selected)
                    {
                        ui.selectedState = i;
                        ui.selectedStates.clear();
                        ui.selectedStates.insert(i);
                        ui.anyStateSelected = false;
                    }
                    else
                    {
                        ui.selectedState = i;
                    }
                }
            }

            if (ui.draggingTransitionLine)
            {
                if (itemHovered)
                {
                    ui.draggingTransitionEntered = true;
                    if (ui.fromStateName != state.name)
                    {
                        ui.toStateName = state.name;
                    }
                }
                else if (ui.toStateName == state.name)
                {
                    ui.draggingTransitionEntered = false;
                    ui.toStateName.clear();
                }
            }

            if (!ui.draggingTransitionLine && !ui.draggingItem && selected && nodeInteraction.dragging)
            {
                ui.draggingItem = true;
                ui.draggingStateIndices.assign(ui.selectedStates.begin(), ui.selectedStates.end());
                if (ui.draggingStateIndices.empty())
                {
                    ui.draggingStateIndices.push_back(i);
                }
            }

            const float grabOffset = 16.0f;
            const UI::NodeGraphNode grabNode = UI::NodeGraph::BuildNode(canvas,
                ImVec2(state.editorPos.x - grabOffset, state.editorPos.y - grabOffset),
                ImVec2(nodeSize.x + grabOffset * 2.0f, nodeSize.y + grabOffset * 2.0f));
            const UI::NodeInteraction grabInteraction = UI::NodeGraph::HandleNode(canvas, "##transition_grab", grabNode);
            anyTransitionGrabHovered |= grabInteraction.hovered;
            canvas.drawList->AddRectFilled(grabNode.screenMin, grabNode.screenMax, grabInteraction.hovered ? IM_COL32(255, 159, 0, 125) : IM_COL32(255, 159, 0, 0),
                20.0f * ui.graphState.zoom);
            if (!ui.draggingTransitionLine && !ui.draggingItem && !itemHovered && grabInteraction.clicked)
            {
                ui.fromStateName = state.name;
                ui.draggingTransitionLine = true;
            }

            const bool isDefault = state.name == animator->defaultState;
            canvas.drawList->AddRectFilled(node.screenMin, node.screenMax, (selected || itemHovered) ? IM_COL32(56, 86, 132, 255) : IM_COL32(38, 46, 57, 255),
                10.0f * ui.graphState.zoom);
            canvas.drawList->AddRect(node.screenMin, node.screenMax, (isDefault || itemHovered) ? IM_COL32(255, 196, 92, 255) : IM_COL32(97, 114, 138, 255),
                10.0f * ui.graphState.zoom, 0, selected ? 2.5f : 1.5f);

            const std::string title = state.name.empty() ? std::format("State {}", i + 1) : state.name;
            canvas.drawList->AddText(ImVec2(node.screenMin.x + 12.0f * ui.graphState.zoom, node.screenMin.y + 12.0f * ui.graphState.zoom), IM_COL32(235, 239, 245, 255), title.c_str());
            canvas.drawList->AddText(ImVec2(node.screenMin.x + 12.0f * ui.graphState.zoom, node.screenMin.y + 34.0f * ui.graphState.zoom), IM_COL32(167, 176, 190, 255), assetManager->GetAssetDisplayName(state.animHandle).c_str());
            if (isDefault)
            {
                canvas.drawList->AddText(ImVec2(node.screenMin.x + 12.0f * ui.graphState.zoom, node.screenMin.y + 52.0f * ui.graphState.zoom), IM_COL32(255, 196, 92, 255), "Default");
            }

            ImGui::PopID();
        }

        if (!ui.draggingTransitionLine && !ui.draggingItem && canvas.hovered && !anyNodeHovered && !anyTransitionGrabHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            ui.boxSelectingStates = true;
            ui.boxSelectStartWorld = UI::NodeGraph::ToWorld(canvas, ImGui::GetIO().MousePos);
            if (!ctrlHeld)
            {
                ui.selectedStates.clear();
                ui.selectedState = -1;
                ui.anyStateSelected = false;
            }
        }

        // Box selection fro animator states and "Any State"
        if (ui.boxSelectingStates)
        {
            const ImVec2 currentWorld = UI::NodeGraph::ToWorld(canvas, ImGui::GetIO().MousePos);
            const ImVec2 minWorld(std::min(ui.boxSelectStartWorld.x, currentWorld.x), std::min(ui.boxSelectStartWorld.y, currentWorld.y));
            const ImVec2 maxWorld(std::max(ui.boxSelectStartWorld.x, currentWorld.x), std::max(ui.boxSelectStartWorld.y, currentWorld.y));

            const ImVec2 selectionMin = UI::NodeGraph::ToScreen(canvas, minWorld);
            const ImVec2 selectionMax = UI::NodeGraph::ToScreen(canvas, maxWorld);
            canvas.drawList->AddRectFilled(selectionMin, selectionMax, IM_COL32(80, 130, 255, 30), 4.0f * ui.graphState.zoom);
            canvas.drawList->AddRect(selectionMin, selectionMax, IM_COL32(126, 170, 255, 220), 4.0f * ui.graphState.zoom, 0, 1.5f);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                ui.boxSelectingStates = false;

                // Any state
                {
                    const ImVec2 anyMin(anyStatePos.x, anyStatePos.y);
                    const ImVec2 anyMax(anyStatePos.x + anyStateNodeSize.x, anyStatePos.y + anyStateNodeSize.y);
                    const bool overlap = !(anyMax.x < minWorld.x || anyMin.x > maxWorld.x || anyMax.y < minWorld.y || anyMin.y > maxWorld.y);
                    if (overlap)
                    {
                        ui.anyStateSelected = true;
                        ui.selectedState = -1;
                    }
                }

                // Animator states
                for (int i = 0; i < static_cast<int>(animator->states.size()); ++i)
                {
                    const AnimState &state = animator->states[static_cast<size_t>(i)];
                    const ImVec2 stateMin(state.editorPos.x, state.editorPos.y);
                    const ImVec2 stateMax(state.editorPos.x + nodeSize.x, state.editorPos.y + nodeSize.y);
                    const bool overlap = !(stateMax.x < minWorld.x || stateMin.x > maxWorld.x || stateMax.y < minWorld.y || stateMin.y > maxWorld.y);
                    if (overlap)
                    {
                        ui.selectedStates.insert(i);
                        ui.selectedState = i;
                    }
                }

                if (ui.selectedStates.empty() && !ui.anyStateSelected)
                {
                    ui.selectedState = -1;
                }
            }
        }

        if (ui.draggingItem)
        {
            const ImVec2 deltaWorld(ImGui::GetIO().MouseDelta.x / ui.graphState.zoom, ImGui::GetIO().MouseDelta.y / ui.graphState.zoom);
            if (deltaWorld.x != 0.0f || deltaWorld.y != 0.0f)
            {
                bool movedAny = false;
                for (const int index : ui.draggingStateIndices)
                {
                    if (index >= 0 && index < static_cast<int>(animator->states.size()))
                    {
                        AnimState &state = animator->states[static_cast<size_t>(index)];
                        state.editorPos.x += deltaWorld.x;
                        state.editorPos.y += deltaWorld.y;
                        movedAny = true;
                    }
                }

                if (ui.anyStateSelected)
                {
                    ui.anyStateEditorPos.x += deltaWorld.x;
                    ui.anyStateEditorPos.y += deltaWorld.y;
                    movedAny = true;
                }

                if (movedAny)
                {
                    animator->SetDirtyFlag(true);
                }
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                ui.draggingItem = false;
                ui.draggingStateIndices.clear();
            }
        }

        if (ui.draggingTransitionLine)
        {
            ImVec2 fromWorldPos = anyStateConnectionPos;
            if (const AnimState *fromState = animator->FindState(ui.fromStateName))
            {
                fromWorldPos = ImVec2(fromState->editorPos.x + nodeSize.x, fromState->editorPos.y + nodeSize.y * 0.5f);
            }

            const ImVec2 start = UI::NodeGraph::ToScreen(canvas, fromWorldPos);
            const ImVec2 end = ImGui::GetMousePos();
            const float handleOffset = 70.0f * ui.graphState.zoom;
            canvas.drawList->AddBezierCubic(start, ImVec2(start.x + handleOffset, start.y), ImVec2(end.x - handleOffset, end.y), end, IM_COL32(255, 196, 92, 255), 2.0f);

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                ui.draggingTransitionLine = false;
                ui.draggingTransitionEntered = false;
                ui.fromStateName.clear();
                ui.toStateName.clear();
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                ui.draggingTransitionLine = false;
                if (!ui.toStateName.empty())
                {
                    auto it = std::ranges::find_if(animator->transitions, [&](const AnimTransition &transition)
                    {
                        return transition.fromState == ui.fromStateName && transition.toState == ui.toStateName;
                    });
                    if (it == animator->transitions.end())
                    {
                        animator->transitions.push_back({ ui.fromStateName, ui.toStateName });
                        animator->SetDirtyFlag(true);
                    }
                }

                ui.draggingTransitionEntered = false;
                ui.fromStateName.clear();
                ui.toStateName.clear();
            }
        }

        canvas.drawList->PopClipRect();
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
                ui.selectedStates.clear();
                if (ui.selectedState >= 0)
                {
                    ui.selectedStates.insert(ui.selectedState);
                    ui.anyStateSelected = false;
                }
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
