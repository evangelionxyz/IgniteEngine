// Copyright (c) 2026 Evangelion Manuhutu

#include "animation_system.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "skeleton.hpp"

namespace ignite
{

    void AnimationSystem::PlayAnimation(std::vector<Ref<SkeletalAnimation>> &animations, int animIndex)
    {
        if (animIndex < animations.size())
        {
            animations[animIndex]->isPlaying = true;
        }
    }

    bool AnimationSystem::UpdateSkeleton(Ref<Skeleton> &skeleton, const Ref<SkeletalAnimation> &animation, float timeInSeconds)
    {
        if (!skeleton || !animation || animation->duration <= 0.0f)
        {
            return false;
        }

        // Find animation key frames
        const float animTime = fmod(timeInSeconds * animation->ticksPerSeconds, animation->duration);

        for (auto &[nodeName, channel] : animation->channels)
        {
            if (const auto it = skeleton->nameToJointMap.find(nodeName); it != skeleton->nameToJointMap.end())
            {
                const i32 jointIndex = it->second;
                skeleton->joints[jointIndex].localTransform = channel.CalculateTransform(animTime, 
                    skeleton->joints[jointIndex].defaultTranslation,
                    skeleton->joints[jointIndex].defaultRotation,
                    skeleton->joints[jointIndex].defaultScale);
            }
        }
        skeleton->UpdateGlobalTransforms();
        return true;
    }
}
