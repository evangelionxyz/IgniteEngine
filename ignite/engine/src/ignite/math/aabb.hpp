// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_AABB_HPP
#define IGN_AABB_HPP

#include "ignite/core/base.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace ignite
{
    class MeshInstance;

    struct IGN_API AABB
    {
        glm::vec3 min = glm::vec3(0.0f);
        glm::vec3 max = glm::vec3(0.0f);

        AABB() = default;
        AABB(const AABB &) = default;
        AABB(const glm::vec3 &center, const glm::vec3 &size)
            : min(center - size * 0.5f), max(center + size * 0.5f)
        {
        }

        const glm::vec3 GetCenter() const
        { 
            return (min + max) / 2.0f; 
        }

        const glm::vec3 GetSize() const
        { 
            return max - min; 
        }

        inline bool RayIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection) const
        {
            float outT;
            return IntersectRay(rayOrigin, rayDirection, outT);
        }

        // Ray intersection that returns distance to intersection in outT (near hit)
        bool IntersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &outT) const;

        AABB Transform(const glm::mat4 &M) const;
    };
}

#endif
