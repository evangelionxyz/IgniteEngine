// Copyright (c) 2026 Evangelion Manuhutu

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

        // TODO: Is Triggered
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

        virtual bool Serialize(const std::filesystem::path &filepath);
        static Ref<AnimationMontage> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::AnimationMontage; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        std::unordered_map<std::string, AnimNotif> m_Notifies;
        AssetHandle m_AnimationHandle = AssetHandle(0);
        AssetHandle m_SkeletonHandle = AssetHandle(0);
    };

    struct BlendSpaceSample
    {
        AssetHandle animationHandle = AssetHandle(0);
        glm::vec2 position = glm::vec2(0.0f);
    };

    class BlendSpace : public Asset
    {
    public:
        std::string name;
        AssetHandle skeletonHandle = AssetHandle(0);
        std::string axisXName = "Speed";
        std::string axisYName = "Direction";
        glm::vec2 axisMin = glm::vec2(0.0f);
        glm::vec2 axisMax = glm::vec2(1.0f);
        std::vector<BlendSpaceSample> samples;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<BlendSpace> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::BlendSpace; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };

    struct LocomotionState
    {
        std::string name;
        bool useBlendSpace = false;
        AssetHandle assetHandle = AssetHandle(0); // .ixanim or .bsp
    };

    class LocomotionController : public Asset
    {
    public:
        std::string name;
        AssetHandle skeletonHandle = AssetHandle(0);
        std::string defaultState;
        std::vector<LocomotionState> states;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<LocomotionController> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::LocomotionController; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };
}

#endif