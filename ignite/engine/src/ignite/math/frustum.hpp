// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_FRUSTUM_HPP
#define IGN_FRUSTUM_HPP

#include "ignite/core/base.hpp"
#include "aabb.hpp"
#include <glm/glm.hpp>
#include <array>

namespace ignite
{
	class ICamera;

    class IGN_API Frustum
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
		Frustum(ICamera *camera);
        Frustum(const glm::mat4 &viewProjection);

        void Update(const glm::mat4 &viewProjection);
        bool IsPointVisible(const glm::vec3 &point) const;
        bool IsSphereVisible(const glm::vec3 &center, float radius) const;
        bool IsAABBVisible(const AABB &aabb) const;
        bool IsAABBVisible(const glm::vec3 &min, const glm::vec3 &max) const;
        void IsAABBVisibleBatch(const AABB *aabbs, size_t count, uint8_t *outVisibilityResults) const;
        const std::array<glm::vec3, 8> &GetCorners() const { return m_Corners; }
        const std::array<glm::vec4, 6> &GetPlanes() const { return m_Planes; }
        std::vector<std::pair<glm::vec3, glm::vec3>> GetEdges() const;

    private:
        std::array<glm::vec4, 6> m_Planes;
        alignas(32) float m_PlanesX[8] = { 0 };
        alignas(32) float m_PlanesY[8] = { 0 };
        alignas(32) float m_PlanesZ[8] = { 0 };
        alignas(32) float m_PlanesW[8] = { 0 };
        std::array<glm::vec3, 8> m_Corners;
        glm::mat4 m_ViewProjection = glm::mat4(1.0f);
        glm::mat4 m_ViewProjectionInverse = glm::mat4(1.0f);
    };
}

#endif