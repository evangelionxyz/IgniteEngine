/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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

#include <glm/glm.hpp>

namespace ignite {

    struct DirLight
    {
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // RGBA
        glm::vec4 direction = { 0.0f, 1.0f, 0.0f, 0.0f }; // Normalized direction
        float intensity = 0.5f;           // Brightness scalar
        float angularSize = 0.1f;         // Simulates soft shadows (sun size in degrees)
        float shadowStrength = 1.0f;      // Strength of cast shadows (0 = no shadow, 1 = full)

        DirLight() { ++count; }
        ~DirLight() { --count; }

        static uint32_t count;
    };

    struct PointLight
    {
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 1.0f };
        float intensity = 1.0f;
        float range = 10.0f;
        float constantAtt = 1.0f;
        float linearAtt = 0.09f;
        float quadraticAtt = 0.032f;

        PointLight() { ++count; }
        ~PointLight() { --count; }

        static uint32_t count;
    };

    struct SpotLight
    {
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 position = { 0.0f, 0.0f, 0.0f, 1.0f };
        glm::vec4 direction = { 0.0f, -1.0f, 0.0f, 0.0f };
        float intensity = 1.0f;
        float range = 10.0f;
        float constantAtt = 1.0f;
        float linearAtt = 0.09f;
        float quadraticAtt = 0.032f;
        float innerConeAngle = 12.5f; // degrees
        float outerConeAngle = 45.0f; // degrees

        SpotLight() { ++count; }
        ~SpotLight() { --count; }

        static uint32_t count;
    };
}
