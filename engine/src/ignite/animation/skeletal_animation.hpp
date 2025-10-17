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

#include "ignite/asset/asset.hpp"
#include "keyframes.hpp"

#include <string>
#include <unordered_map>

namespace ignite {

    struct AnimationNode
    {
        std::string name;
        glm::mat4 transformation;
        AnimationNode *parent = nullptr;
        std::vector<AnimationNode> children;
	};
    
    class AnimationChannel
    {
    public:
        AnimationChannel() = default;
        AnimationChannel(const AnimationNode *animNode);

        // time in seconds * ticks per second
        // S * (T/S)
        glm::mat4 CalculateTransform(float timeInTicks);

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

        std::unordered_map<std::string, AnimationChannel> channels;

        static AssetType GetStaticType() { return AssetType::SkeletalAnimation; }
        virtual AssetType GetType() override { return GetStaticType(); }
    };
}
