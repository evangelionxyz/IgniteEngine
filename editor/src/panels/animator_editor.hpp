// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATOR_EDITOR_HPP
#define ANIMATOR_EDITOR_HPP

#include "../node/node_graph.hpp"

#include "ignite/animation/animator/animator_controller.hpp"

#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>

namespace ignite
{
    class AssetManager;

    struct AnimatorControllerEditorState
    {
        UI::NodeGraphState graphState;
        int selectedState = -1;
        int selectedTransition = -1;
        bool initialized = false;
        bool draggingItem = false;
        bool boxSelectingStates = false;
        std::unordered_set<int> selectedStates;
        std::vector<int> draggingStateIndices;
        ImVec2 boxSelectStartWorld = ImVec2(0.0f, 0.0f);
        glm::vec2 anyStateEditorPos = glm::vec2(20.0f, 20.0f);
        bool anyStateSelected = false;
        bool draggingAnyState = false;

        // Transition
        std::string fromStateName;
        std::string toStateName;
        bool draggingTransitionEntered = false;
        bool draggingTransitionLine = false;
    };

    class AnimatorEditor
    {
    public:
        static bool DrawAnimatorStateCombo(const char *label, const std::vector<AnimState> &states, std::string &value, bool allowAnyState = false);
        static bool DrawAnimatorParamCombo(const char *label, const std::vector<AnimParam> &params, std::string &value);

        static void RenameAnimatorStateReferences(const Ref<AnimatorController> &animator, const std::string &oldName, const std::string &newName);
        static void RemoveAnimatorStateReferences(const Ref<AnimatorController> &animator, const std::string &stateName);
        static void RemoveAnimatorParamReferences(const Ref<AnimatorController> &animator, const std::string &paramName);
        static void DrawAnimatorControllerLeftPanel(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui);
        
        // Graph
        static void DrawAnimatorControllerGraph(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui);
        
        static void DrawAnimatorControllerInspectorTab(const Ref<AnimatorController> &animator, AssetManager *assetManager, AnimatorControllerEditorState &ui);
        static void DrawAnimatorControllerConditions(const Ref<AnimatorController> &animator, AnimatorControllerEditorState &ui);
        static void DrawAnimatorControllerParamsTab(const Ref<AnimatorController> &animator);
    };
}

#endif
