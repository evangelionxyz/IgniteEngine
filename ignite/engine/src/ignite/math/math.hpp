// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_MATH_HPP
#define IGN_MATH_HPP

#include "ignite/core/base.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/hash.hpp>

#include "obb.hpp"
#include "ignite/physics/3d/physics_3d.hpp"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"

namespace ignite
{
    class ICamera;
    struct Rect;

    class IGN_API Math
    {
    public:
        static bool DecomposeTransform(const glm::mat4 &transform, glm::vec3 &outTranslation, glm::quat &outRotation, glm::vec3 &outScale);
        static bool DecomposeTransformEuler(const glm::mat4 &transform, glm::vec3 &outTranslation, glm::vec3 &outRotation, glm::vec3 &outScale);
        static bool ProjectWorldToScreen(const glm::vec3 &worldPosition, const glm::mat4 &viewProjection, const Rect &viewportRect, ImVec2 &outScreen);
        static bool RaySphereIntersection(const physics::Ray &ray, const glm::vec3 &sphereCenter, float sphereRadius);
        static bool RayPlaneIntersection(const physics::Ray &ray, const glm::vec3 &planeNormal, const glm::vec3 &planePoint, float &t);
        static bool RayQuadIntersection(const physics::Ray &ray, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, const glm::vec3 &v3, float &t);
        static glm::vec3 Normalize(const glm::vec3 &v);
        static glm::vec3 WorldToScreen(const glm::vec3 &worldPosition, const glm::mat4 &modelTransform, const glm::mat4 &viewProjection, const glm::vec2 &screenSize);
        static glm::vec2 GetNormalizedDeviceCoord(const glm::vec2 &mouse, const glm::vec2 &screen);
        static glm::vec4 GetEyeCoord(glm::vec4 clipCoords, const glm::mat4 &projectionMatrix);
        static glm::vec3 GetWorldPosition(const glm::vec4 &eyeCoords, const glm::mat4 &view);
        static glm::vec3 GetRayFromScreenCoords(const glm::vec2 &coord, const glm::vec2 &screen, const glm::mat4 &projection, const glm::mat4 &view, bool isPerspective, glm::vec3 *rayOrigin);
        static glm::vec3 GetRayFromScreenCoords(const glm::vec2 &coord, const glm::vec2 &screen, ICamera *camera, glm::vec3 *outOrigin);

        static glm::vec3 ScreenToWorldOnPlane(const glm::vec2 &screenPos, float planeZ, const glm::mat4 &viewProjection, const Rect &viewportRect, bool *isValid = nullptr);
        static glm::mat4 RemoveScale(const glm::mat4 &matrix);
        static void ComputeCascadeMatrices(const glm::vec3 &cameraPosition, const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 lightDriection, int32_t cascadeCount, const std::vector<float> &cascade_splits, std::vector<glm::mat4> &cascade_light_matrices);
        static void ExtractFrustumCorners(const glm::mat4 &view, const glm::mat4 &projection, float nearPlane, float farPlane, glm::vec4 outCorners[8]);
        static float CascadeSplit(int32_t index, int32_t cascade_count, float near_plane, float far_plane, float lambda);
        static glm::vec3 SnapToGrid(glm::vec3 position, float texel_size);
    };

    struct IGN_API Rect
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

	using Viewport = Rect;

    struct IGN_API Margin
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

#endif
