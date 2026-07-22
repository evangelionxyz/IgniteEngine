// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ANIMATION_EDITOR_HPP
#define IGN_ANIMATION_EDITOR_HPP

#include "ignite/core/types.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "panels/asset_editor_data.hpp"

#include <vector>

namespace ignite
{
    class AssetManager;

    struct AnimationEditorState
    {
        AssetHandle previewMeshHandle = AssetHandle(0);
        float timeSeconds = 0.0f;
        float timelineHeight = 100.0f;
        float previewColumnWidth = 0.0f;
        int selectedJoint = -1;
        int selectedKeyframeType = 0;   // 0=Translation, 1=Rotation, 2=Scale
        int selectedKeyframeIndex = -1;
        int selectedEventIndex = -1;
        bool playing = false;
        bool loop = false;
    };

    class AnimationEditor
    {
    public:
        static void Draw( const Ref<SkeletalAnimation> &animation, AssetManager *assetManager,
            UI::EditorSceneData &sceneData, AnimationEditorState &state, float deltaTime);
    };
}

#endif
