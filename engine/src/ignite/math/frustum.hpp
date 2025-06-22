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

#include <glm/glm.hpp>
#include <array>

namespace ignite {
    class Frustum
    {
    public:
        enum class Plane
        {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far
        };

        Frustum() = default;
        Frustum(const glm::mat4 &view_projection);

        void Update(const glm::mat4 &view_projection);
        bool IsPointVisible(const glm::vec3 &point) const;
        bool IsSphereVisible(const glm::vec3 &center, float radius) const;
        bool IsAABBVisible(const glm::vec3 &min, const glm::vec3 &max) const;
        const std::array<glm::vec3, 8> &GetCorners() const { return m_Corners; }
        const std::array<glm::vec4, 6> &GetPlanes() const { return m_Planes; }
        std::vector<std::pair<glm::vec3, glm::vec3>> GetEdges() const;

    private:
        std::array<glm::vec4, 6> m_Planes;
        std::array<glm::vec3, 8> m_Corners;
        glm::mat4 m_ViewProjectionInverse;
    };
}
