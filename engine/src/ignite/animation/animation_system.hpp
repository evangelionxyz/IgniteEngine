// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#include "ignite/graphics/objects/mesh.hpp"
#include "skeletal_animation.hpp"
#include "ignite/core/types.hpp"

namespace ignite {
    
    class Model;
    class Skeleton;

    class AnimationSystem
    {
    public:
        static void PlayAnimation(std::vector<Ref<SkeletalAnimation>> &animations, int animIndex = 0);
        static void ApplySkeletonToEntities(Scene *scene, Ref<Skeleton> &skeleton); 
        static bool UpdateSkeleton(Ref<Skeleton> &skeleton, const Ref<SkeletalAnimation> &animation, float timeInSeconds);
        static void UpdateGlobalTransforms(const Ref<Skeleton> &skeleton);
        static std::vector<glm::mat4> GetFinalJointTransforms(const Ref<Skeleton> &skeleton);
        static void GetFinalJointTransforms(const Ref<Skeleton> &skeleton, std::vector<glm::mat4> &outTransforms);
    };
}
