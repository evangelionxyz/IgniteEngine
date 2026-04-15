// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATOR_EDITOR_HPP
#define ANIMATOR_EDITOR_HPP

#include "ignite/animation/animator/animator_controller.hpp"

#include <string>
#include <vector>

#include <imgui.h>

namespace ignite
{
    class AssetManager;

    struct AnimatorControllerEditorState
    {
        ImVec2 canvasOffset = ImVec2(36.0f, 36.0f);
        int selectedState = -1;
        int selectedTransition = -1;
        bool initialized = false;
        bool draggingItem = false;

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