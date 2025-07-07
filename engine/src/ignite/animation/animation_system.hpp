/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "ignite/graphics/mesh.hpp"
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
    };
}
