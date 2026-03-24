// Copyright (c) 2026 Evangelion Manuhutu

#include "animation_system.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "skeleton.hpp"

namespace ignite {

    void AnimationSystem::PlayAnimation(std::vector<Ref<SkeletalAnimation>> &animations, int animIndex)
    {
        if (animIndex < animations.size())
        {
            animations[animIndex]->isPlaying = true;
        }
    }

    void AnimationSystem::ApplySkeletonToEntities(Scene* scene, Ref<Skeleton> &skeleton)
    {
        for (size_t i = 0; i < skeleton->joints.size(); i++)
        {
            auto it = skeleton->jointEntityMap.find(static_cast<i32>(i));
            if (it == skeleton->jointEntityMap.end())
                continue;

            Entity entity = SceneManager::GetEntity(scene, it->second);

            if (!entity.IsValid() || !entity.HasComponent<TransformComponent>())
                continue;
            
            glm::vec3 skew;
            glm::vec4 perspective;

            TransformComponent& transform = entity.GetTransform();
            glm::decompose(skeleton->joints[i].localTransform,
                transform.localScale,
                transform.localRotation,
                transform.localTranslation,
                skew,
                perspective);

            transform.isAnimated = true;
            transform.dirty = true;
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

        UpdateGlobalTransforms(skeleton);
        return true;
    }

    void AnimationSystem::UpdateGlobalTransforms(const Ref<Skeleton> &skeleton)
    {
        // Important optimization: Calculate global transforms in hierarchy order
        for (size_t i = 0; i < skeleton->joints.size(); ++i)
        {
            Joint &joint = skeleton->joints[i];

            if (joint.parentJointId == -1)
            {
                // Root joint
                joint.globalTransform = joint.localTransform;
            }
            else
            {
                // Child joint
                joint.globalTransform = skeleton->joints[joint.parentJointId].globalTransform * joint.localTransform;
            }
        }
    }

    std::vector<glm::mat4> AnimationSystem::GetFinalJointTransforms(const Ref<Skeleton> &skeleton)
    {
        std::vector<glm::mat4> finalTransforms;
        GetFinalJointTransforms(skeleton, finalTransforms);

        return finalTransforms;
    }

    void AnimationSystem::GetFinalJointTransforms(const Ref<Skeleton> &skeleton, std::vector<glm::mat4> &outTransforms)
    {
        if (!skeleton)
        {
            outTransforms.clear();
            return;
        }

        outTransforms.resize(skeleton->joints.size());

        for (size_t i = 0; i < skeleton->joints.size(); ++i)
        {
            const Joint &joint = skeleton->joints[i];
            outTransforms[i] = joint.globalTransform * joint.inverseBindPose;
        }
    }

}
