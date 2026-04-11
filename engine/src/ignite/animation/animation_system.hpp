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
        static bool UpdateSkeleton(Ref<Skeleton> &skeleton, const Ref<SkeletalAnimation> &animation, float timeInSeconds);
    };
}
