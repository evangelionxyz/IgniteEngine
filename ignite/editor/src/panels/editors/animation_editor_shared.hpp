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

namespace ignite
{
    class Gizmo;
}

namespace ignite::UI
{
    struct AnimPreviewViewport
    {
        static void Draw(EditorSceneData &sceneData, float deltaTime);

        static void DrawOverlay(
            EditorSceneData &sceneData,
            const Ref<Skeleton> &skeleton,
            const std::vector<glm::mat4> *previewGlobalTransforms,
            int32_t &selectedJoint,
            int32_t &selectedSocket,
            int gizmoTarget,
            Gizmo &gizmo,
            bool &isDirty
        );
    };

    struct AnimTimelineTrack
    {
        // Draw base timeline with optional event markers from SkeletalAnimation and NotifyCallbacks
        static void Draw(ImDrawList *dl, float tlHeight, float totalDuration, float *playbackTime, bool *isPlaying,
            const std::vector<AnimationTimelineEvent> *sourceEvents = nullptr, const std::vector<AnimNotifyCallback> *notifyCallbacks = nullptr,
            int *selectedCallbackIndex = nullptr, int *selectedEventIndex = nullptr, const char *emptyMessage = "No animation assigned");

        static void Draw(ImDrawList *dl, float tlHeight, float totalDuration, float *playbackTime, bool *isPlaying,
            const std::vector<AnimationTimelineEvent> *sourceEvents, const std::vector<AnimNotifyCallback> *notifyCallbacks,
            int *selectedCallbackIndex, const char *emptyMessage)
        {
            Draw(dl, tlHeight, totalDuration, playbackTime, isPlaying, sourceEvents, notifyCallbacks, selectedCallbackIndex, nullptr, emptyMessage);
        }
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
