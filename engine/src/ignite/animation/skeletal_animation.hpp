// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SKELETAL_ANIMATION_HPP
#define SKELETAL_ANIMATION_HPP

#include "ignite/asset/asset.hpp"
#include "animator/animator.hpp"
#include "keyframes.hpp"

#include <string>
#include <unordered_map>

namespace ignite
{
    
    class AnimationChannel
    {
    public:
        AnimationChannel() = default;

        // S * (T/S)
        TRS CalculateTRS(float timeInTicks, const glm::vec3& defaultTranslation, const glm::quat& defaultRotation, const glm::vec3& defaultScale);

        glm::mat4 CalculateTransform(float timeInTicks, const glm::vec3 &defaultTranslation, const glm::quat &defaultRotation, const glm::vec3 &defaultScale);

        Vec3Key translationKeys;
        QuatKey rotationKeys;
        Vec3Key scaleKeys;

        glm::vec3 translation;
        glm::vec3 scale;
        glm::quat rotation;
    };

    class SkeletalAnimation : public Asset
    {
    public:
        SkeletalAnimation() = default;

        std::string name;
        float duration = 0;
        float ticksPerSeconds = 1.0f;
        float timeInSeconds = 0.0f;
        bool isPlaying = false;
        std::unordered_map<int, AnimationChannel> channels;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<SkeletalAnimation> Deserialize(const std::filesystem::path &filepath);

        void SetSkeletonHandle(UUID skeletonHandle);
        UUID GetSkeletonHandle() { return m_SkeletonHandle; }

        static AssetType GetStaticType() { return AssetType::SkeletalAnimation; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    
    private:
        UUID m_SkeletonHandle = UUID(0);
    };
}

#endif
