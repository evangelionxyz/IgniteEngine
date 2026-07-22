// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATION_MONTAGE_EDITOR_HPP
#define ANIMATION_MONTAGE_EDITOR_HPP

#include "ignite/core/types.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "panels/asset_editor_data.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace ignite
{
    class AssetManager;

    struct AnimationMontageEditorState
    {
        float timeSeconds = 0.0f;
        float timelineHeight = 180.0f;
        float previewColumnWidth = 0.0f;
        int selectedNotifyIndex = -1;
        int selectedCallbackIndex = -1;
        bool playing = false;
        bool loop = false;
    };

    class AnimationMontageEditor
    {
    public:
        static void Draw(const Ref<AnimationMontage> &montage, AssetManager *assetManager,
            UI::EditorSceneData &sceneData, AnimationMontageEditorState &state, float deltaTime);
    };
}

#endif
