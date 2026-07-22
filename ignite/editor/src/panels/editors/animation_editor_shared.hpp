// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ANIMATION_EDITOR_SHARED_HPP
#define IGN_ANIMATION_EDITOR_SHARED_HPP

#include "ignite/core/types.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/graphics/texture.hpp"
#include "panels/asset_editor_data.hpp"

#include <imgui.h>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace ignite::UI
{
    struct AnimPreviewViewport
    {
        static void Draw(EditorSceneData &sceneData, float deltaTime);
    };

    struct AnimTimelineTrack
    {
        // Draw base timeline with optional event markers from SkeletalAnimation and NotifyCallbacks
        static void Draw(ImDrawList *dl, float tlHeight, float totalDuration, float *playbackTime, bool *isPlaying,
            const std::vector<AnimationTimelineEvent> *sourceEvents = nullptr, const std::vector<AnimNotifyCallback> *notifyCallbacks = nullptr,
            int *selectedCallbackIndex = nullptr, const char *emptyMessage = "No animation assigned");
    };

    struct SkeletonBodyPartSelector
    {
        static void Draw(const Ref<Skeleton> &skeleton, std::vector<int32_t> &maskedJoints, bool &dirty);
    };

    struct AnimPlaybackControls
    {
        static void Draw(bool &playing, bool &loop, float &timeSeconds, float totalDuration, bool enabled = true);
    };
}

#endif
