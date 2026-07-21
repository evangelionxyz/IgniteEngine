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
        AssetHandle audioHandle = AssetHandle(0);       // optional one-shot override
        AssetHandle callbackAsset = AssetHandle(0);     // AnimationTimelineCallback .ixso
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

        std::string name;
        float duration = 0;
        float ticksPerSeconds = 1.0f;
        float timeInSeconds = 0.0f;
        bool isPlaying = false;
        std::unordered_map<int, AnimationChannel> channels;
        std::vector<AnimationTimelineEvent> timelineEvents;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<SkeletalAnimation> Deserialize(const ignite::Path &filepath);

        void SetSkeletonHandle(UUID skeletonHandle);
        UUID GetSkeletonHandle() const { return m_SkeletonHandle; }

        static AssetType GetStaticType() { return AssetType::SkeletalAnimation; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    
    private:
        UUID m_SkeletonHandle = UUID(0);
    };
}

#endif
