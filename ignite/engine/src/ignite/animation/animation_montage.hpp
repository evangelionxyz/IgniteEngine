// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ANIMATION_MONTAGE_HPP
#define IGN_ANIMATION_MONTAGE_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/animation/skeletal_animation.hpp"

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace ignite
{
    class SkeletalAnimation;

    struct IGN_API AnimNotif
    {
        AnimNotif() = default;
        AnimNotif(float startTime, float endTime)
            : startTime(startTime), endTime(endTime)
        { }

        float startTime = -1.0f;
        float endTime = -1.0f;

        void OnUpdate(float currentTime)
        {
            m_InRange = currentTime >= startTime && currentTime <= endTime;
        }

        [[nodiscard]] bool IsInRange() const { return m_InRange; }

    private:
        bool m_InRange = false;
    };

    // Notify callback fired at a specific timestep during montage playback
    struct IGN_API AnimNotifyCallback
    {
        AnimNotifyCallback() = default;
        AnimNotifyCallback(float timestep, AnimationTimelineEvent::Action actionType, const std::string &callbackName)
            : timestep(timestep), actionType(actionType), callbackName(callbackName)
        { }

        float timestep = 0.0f;                                     // normalized time [0..1]
        AnimationTimelineEvent::Action actionType = AnimationTimelineEvent::Action::ScriptCallback;
        std::string callbackName;                                   // function name for script callbacks
        AssetHandle audioHandle = AssetHandle(0);                   // optional audio asset for Action::Audio

        // Runtime state - not serialized
        void OnUpdate(float normalizedTime, float tolerance = 0.01f)
        {
            bool wasActive = m_Active;
            m_Active = std::abs(normalizedTime - timestep) <= tolerance;
            m_JustTriggered = m_Active && !wasActive;
        }

        [[nodiscard]] bool IsActive() const { return m_Active; }
        [[nodiscard]] bool JustTriggered() const { return m_JustTriggered; }

    private:
        bool m_Active = false;
        bool m_JustTriggered = false;
    };

    class IGN_API AnimationMontage : public Asset
    {
    public:
        std::string name;

        // AnimNotif (range-based notifications)
        void AddNotif(const std::string &name, float startTime, float endTime);
        void SetNotif(const std::string &name, const AnimNotif &notif);
        void RemoveNotif(const std::string &name);

        AnimNotif GetAnimNotif(const std::string &name);
        std::unordered_map<std::string, AnimNotif> &GetAnimNotifies() { return m_Notifies; }
        const std::unordered_map<std::string, AnimNotif> &GetAnimNotifies() const { return m_Notifies; }

        // AnimNotifyCallback (timestep-based function callbacks)
        void AddNotifyCallback(float timestep, AnimationTimelineEvent::Action actionType, const std::string &callbackName);
        void RemoveNotifyCallback(size_t index);
        std::vector<AnimNotifyCallback> &GetNotifyCallbacks() { return m_NotifyCallbacks; }
        const std::vector<AnimNotifyCallback> &GetNotifyCallbacks() const { return m_NotifyCallbacks; }

        // Animation handle with pin tracking
        void SetAnimationHandle(AssetHandle animationHandle);
        AssetHandle GetAnimationHandle() const { return m_AnimationHandle; }

        // Skeleton handle with pin tracking
        void SetSkeletonHandle(AssetHandle skeletonHandle);
        AssetHandle GetSkeletonHandle() const { return m_SkeletonHandle; }

        // Body part mask: joint indices affected by this montage
        void SetMaskedJoints(const std::vector<int32_t> &joints) { m_MaskedJoints = joints; SetDirtyFlag(true); }
        const std::vector<int32_t> &GetMaskedJoints() const { return m_MaskedJoints; }

        virtual bool Serialize(const ignite::Path &filepath);
        static Ref<AnimationMontage> Deserialize(const ignite::Path &filepath);

        static AssetType GetStaticType() { return AssetType::AnimationMontage; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        std::unordered_map<std::string, AnimNotif> m_Notifies;
        std::vector<AnimNotifyCallback> m_NotifyCallbacks;
        std::vector<int32_t> m_MaskedJoints;
        AssetHandle m_AnimationHandle = AssetHandle(0);
        AssetHandle m_SkeletonHandle = AssetHandle(0);
    };
}

#endif