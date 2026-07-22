// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SKELETAL_ANIMATION_HPP
#define IGN_SKELETAL_ANIMATION_HPP

#include "ignite/asset/asset.hpp"
#include "animator/animator.hpp"
#include "keyframes.hpp"

#include "ignite/math/transform.hpp"

#include <string>
#include <unordered_map>

namespace ignite
{
    struct IGN_API AnimationTimelineEvent
    {
        enum class Action : uint8_t { Audio, ScriptCallback };

        float normalizedTime = 0.0f;
        std::string name = "Event";
        Action action = Action::ScriptCallback;

        AnimationTimelineEvent();
        ~AnimationTimelineEvent();

        AnimationTimelineEvent(const AnimationTimelineEvent &other);
        AnimationTimelineEvent &operator=(const AnimationTimelineEvent &other);

        AnimationTimelineEvent(AnimationTimelineEvent &&other) noexcept;
        AnimationTimelineEvent &operator=(AnimationTimelineEvent &&other) noexcept;

        void SetAudioHandle(const AssetHandle &handle);
        const AssetHandle &GetAudioHandle() const { return m_AudioHandle; }

        void SetCallbackAsset(const AssetHandle &handle);
        const AssetHandle &GetCallbackAsset() const { return m_CallbackAsset; }

        const UUID &GetUUID() const { return m_UUID; }

    private:
        void UnpinAssets();

        UUID m_UUID;
        AssetHandle m_AudioHandle = AssetHandle(0);
        AssetHandle m_CallbackAsset = AssetHandle(0);
    };

    class IGN_API AnimationChannel
    {
    public:
        AnimationChannel() = default;

        // S * (T/S)
        Transform Calculate(float timeInTicks, const Transform& defaultTransform);

        Vec3Key translationKeys;
        QuatKey rotationKeys;
        Vec3Key scaleKeys;
    };

    class IGN_API SkeletalAnimation : public Asset
    {
    public:
        SkeletalAnimation() = default;

        ~SkeletalAnimation();

        std::string name;
        float duration = 0;
        float ticksPerSeconds = 1.0f;
        float timeInSeconds = 0.0f;
        bool isPlaying = false;
        std::unordered_map<int, AnimationChannel> channels;
        std::vector<AnimationTimelineEvent> timelineEvents;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<SkeletalAnimation> Deserialize(const ignite::Path &filepath);

        void SetSkeletonHandle(const AssetHandle &skeletonHandle);
        const AssetHandle &GetSkeletonHandle() const { return m_SkeletonHandle; }

        static AssetType GetStaticType() { return AssetType::SkeletalAnimation; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    
    private:
        AssetHandle m_SkeletonHandle = AssetHandle(0);
    };
}

#endif
