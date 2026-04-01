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

#include "ignite/scene/icamera.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "obb.hpp"

namespace ignite
{
    class TransformComponent;
    struct Rect;

    class Math
    {
    public:
        static glm::mat4 ComposeTransformComponent(const TransformComponent &transform);
        static void DecomposeTransformComponent(const glm::mat4 &matrix, TransformComponent &outTransform);

        static bool DecomposeTransform(const glm::mat4 &transform, glm::vec3 &outTranslation, glm::quat &outRotation, glm::vec3 &out_scale);
        static bool DecomposeTransformEuler(const glm::mat4 &transform, glm::vec3 &outTranslation, glm::vec3 &outRotation, glm::vec3 &out_scale);

        static glm::vec3 Normalize(const glm::vec3 &v);
        static glm::vec3 WorldToScreen(const glm::vec3 &world_position, const glm::mat4 &model_transform, const glm::mat4 &view_projection, const glm::vec2 &screen_size);
        static bool ProjectWorldToScreen(const glm::vec3 &worldPos, const glm::mat4 &viewProjection, const Rect &viewportRect, ImVec2 &outScreen);
        static glm::vec2 GetNormalizedDeviceCoord(const glm::vec2 &mouse, const glm::vec2 &screen);
        static glm::vec4 GetEyeCoord(glm::vec4 clipCoords, const glm::mat4 &projectionMatrix);
        static glm::vec3 GetWorldPosition(const glm::vec4 &eyeCoords, const glm::mat4 &viewMatrix);
        static glm::vec3 GetRayFromScreenCoords(const glm::vec2 &coord, const glm::vec2 &screen, const glm::mat4 &projection, const glm::mat4 &view, bool isPerspective, glm::vec3 &rayOrigin);
        static glm::vec3 ScreenToWorldOnPlane(const glm::vec2 &screenPos, float planeZ, const glm::mat4 &viewProjection, const Rect &viewportRect, bool *isValid = nullptr);
        static bool RaySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const glm::vec3 &sphereCenter, float sphereRadius);

        static glm::mat4 RemoveScale(const glm::mat4 &matrix);

        static float CascadeSplit(i32 index, i32 cascade_count, f32 near_plane, f32 far_plane, f32 lambda);
        static void ComputeCascadeMatrices(const glm::vec3 &camera_pos, const glm::mat4 &camera_view, const glm::mat4 &camera_projection, const glm::vec3 light_direction, i32 cascade_count, const std::vector<f32> &cascade_splits, std::vector<glm::mat4> &cascade_light_matrices);
        static void ExtractFrustumCorners(const glm::mat4 &view, const glm::mat4 &projection, f32 near_plane, f32 far_plane, glm::vec4 out_corners[8]);
        static glm::vec3 SnapToGrid(glm::vec3 position, f32 texel_size);
    };

    struct Rect
    {
        glm::vec2 min;
        glm::vec2 max;

        Rect() : min(0.0f, 0.0f), max(0.0f, 0.0f) { }
        Rect(glm::vec2 min, glm::vec2 max) : min(min), max(max) { }
        Rect(float min_x, float min_y, float max_x, float max_y) : min(min_x, min_y), max(max_x, max_y) { }

        inline Rect operator+(const Rect &rhs) const
        {
            return { min + rhs.min, max + rhs.max };
        }

        inline Rect operator-(const Rect &rhs) const
        {
            return { min - rhs.min, max - rhs.max };
        }

        inline Rect operator*(const Rect &rhs) const 
        {
            return { min * rhs.min, max * rhs.max };
        }

        inline Rect operator/(const Rect &rhs) const
        {
            return { min / rhs.min, max / rhs.max };
        }

        const bool Contains(const glm::vec2 &p) const
        {
            return p.x >= min.x && p.y >= min.y && p.x <= max.x && p.y <= max.y;
        }
        const bool Contains(const ImVec2 &p) const
        {
            return p.x >= min.x && p.y >= min.y && p.x <= max.x && p.y <= max.y;
        }

        void SetMin(const glm::vec2 &min_) { this->min = min_; }
        void SetMin(float x, float y) { min.x = x; min.y = y; }
        void SetMax(const glm::vec2 &max_) { this->max = max_; }
        void SetMax(float x, float y) { max.x = x; max.y = y; }

        const glm::vec2 GetCenter() const 
        { 
            return { (min.x + max.x) / 2.0f, (min.y + max.y) / 2.0f };
        }

        const glm::vec2 GetSize() const
        {
            return { max.x - min.x, max.y - min.y };
        }
    };

    struct Margin
    {
        float top, bottom, left, right;
        Margin() : top(0.0f), bottom(0.0f), left(0.0f), right(0.0f) {}
        Margin(float value) : top(value), bottom(value), left(value), right(value) {}
        Margin(float horizontal, float vertical) : top(vertical), bottom(vertical), left(horizontal), right(horizontal) {}
        Margin(float l, float r, float t, float b) : top(t), bottom(b), left(l), right(r) {}

        glm::vec2 Start() const { return { left, top }; }
        glm::vec2 End() const { return { right, bottom }; }
    };

}
