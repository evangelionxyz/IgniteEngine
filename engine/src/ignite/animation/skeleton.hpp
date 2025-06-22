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

#include "ignite/core/uuid.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace ignite
{
    struct Joint
    {
        std::string name;
        int32_t id; // index in joints array
        int32_t parentJointId; // parent in skeleton hierarchy (-1 for root)
        glm::mat4 inverseBindPose; // inverse bind pose matrix
        glm::mat4 localTransform; // current local transform
        glm::mat4 globalTransform; // current global transform
    };

    struct Skeleton
    {
        std::vector<Joint> joints;
        std::unordered_map<std::string, int32_t> nameToJointMap; // for fast lookup by name
        std::unordered_map<int32_t, UUID> jointEntityMap;
    };
}
