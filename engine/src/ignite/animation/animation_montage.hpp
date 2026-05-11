// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATION_MONTAGE_HPP
#define ANIMATION_MONTAGE_HPP

#include "ignite/asset/asset.hpp"

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace ignite
{
    class SkeletalAnimation;

    struct AnimNotif
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

    class AnimationMontage : public Asset
    {
    public:
        std::string name;

        void AddNotif(const std::string &name, float startTime, float endTime);
        void SetNotif(const std::string &name, const AnimNotif &notif);
        void RemoveNotif(const std::string &name);

        AnimNotif GetAnimNotif(const std::string &name);
        std::unordered_map<std::string, AnimNotif> &GetAnimNotifies() { return m_Notifies; }
        const std::unordered_map<std::string, AnimNotif> &GetAnimNotifies() const { return m_Notifies; }

        void SetAnimationHandle(AssetHandle animationHandle) { m_AnimationHandle = animationHandle; }
        AssetHandle GetAnimationHandle() const { return m_AnimationHandle; }

        void SetSkeletonHandle(AssetHandle skeletonHandle) { m_SkeletonHandle = skeletonHandle; }
        AssetHandle GetSkeletonHandle() const { return m_SkeletonHandle; }

        virtual bool Serialize(const ignite::Path &filepath);
        static Ref<AnimationMontage> Deserialize(const ignite::Path &filepath);

        static AssetType GetStaticType() { return AssetType::AnimationMontage; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        std::unordered_map<std::string, AnimNotif> m_Notifies;
        AssetHandle m_AnimationHandle = AssetHandle(0);
        AssetHandle m_SkeletonHandle = AssetHandle(0);
    };
}

#endif