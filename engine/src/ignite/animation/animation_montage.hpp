// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATION_MONTAGE_HPP
#define ANIMATION_MONTAGE_HPP

#include "ignite/asset/asset.hpp"

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
            m_InRange = startTime >= currentTime && endTime <= endTime;
        }

        [[nodiscard]] bool IsInRange() const { return m_InRange; }

        // TODO: Is Triggered
    private:
        bool m_InRange = false;
    };

    class AnimationMontage : public Asset
    {
    public:
        void AddNotif(const std::string &name, float startTime, float endTime);
        void SetNotif(const std::string &name, const AnimNotif &notif);
        void RemoveNotif(const std::string &name);

        AnimNotif GetAnimNotif(const std::string &name);
        std::unordered_map<std::string, AnimNotif> &GetAnimNotifies() { return m_Notifies; }

        virtual bool Serialize(const std::filesystem::path &filepath);
        static Ref<AnimationMontage> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::AnimationMontage; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        std::unordered_map<std::string, AnimNotif> m_Notifies;
        Ref<SkeletalAnimation> m_Animation;
    };
}

#endif