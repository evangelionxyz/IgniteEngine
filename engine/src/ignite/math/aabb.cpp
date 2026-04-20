// Copyright (c) 2026 Evangelion Manuhutu

#include "aabb.hpp"

#include "ignite/graphics/objects/mesh.hpp"

namespace ignite
{
    AABB AABB::CalculateMeshAABB(const std::vector<Ref<MeshInstance>> &meshInstances)
    {
        AABB bounds;
        bool hasBounds = false;

        for (const Ref<MeshInstance> &mesh : meshInstances)
        {
            if (!mesh || !mesh->GetPrimitive() || mesh->GetPrimitive()->vertices.empty())
            {
                continue;
            }

            const AABB &primitiveBounds = mesh->GetPrimitive()->aabb;
            const glm::vec3 corners[8] =
            {
                { primitiveBounds.min.x, primitiveBounds.min.y, primitiveBounds.min.z },
                { primitiveBounds.max.x, primitiveBounds.min.y, primitiveBounds.min.z },
                { primitiveBounds.min.x, primitiveBounds.max.y, primitiveBounds.min.z },
                { primitiveBounds.max.x, primitiveBounds.max.y, primitiveBounds.min.z },
                { primitiveBounds.min.x, primitiveBounds.min.y, primitiveBounds.max.z },
                { primitiveBounds.max.x, primitiveBounds.min.y, primitiveBounds.max.z },
                { primitiveBounds.min.x, primitiveBounds.max.y, primitiveBounds.max.z },
                { primitiveBounds.max.x, primitiveBounds.max.y, primitiveBounds.max.z },
            };

            AABB meshBounds;
            meshBounds.min = glm::vec3(std::numeric_limits<float>::max());
            meshBounds.max = glm::vec3(std::numeric_limits<float>::lowest());

            for (const glm::vec3 &corner : corners)
            {
                const glm::vec4 world = mesh->local * glm::vec4(corner, 1.0f);
                meshBounds.min = glm::min(meshBounds.min, glm::vec3(world));
                meshBounds.max = glm::max(meshBounds.max, glm::vec3(world));
            }

            if (!hasBounds)
            {
                bounds = meshBounds;
                hasBounds = true;
                continue;
            }

            bounds.min = glm::min(bounds.min, meshBounds.min);
            bounds.max = glm::max(bounds.max, meshBounds.max);
        }

        return bounds;
    }

    bool AABB::IntersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, float &outT) const
    {
        // Slab method
        const glm::vec3 invDir = 1.0f / rayDirection;
        glm::vec3 t0s = (min - rayOrigin) * invDir;
        glm::vec3 t1s = (max - rayOrigin) * invDir;

        glm::vec3 tsmaller = glm::min(t0s, t1s);
        glm::vec3 tbigger = glm::max(t0s, t1s);

        float tmin = glm::max(glm::max(tsmaller.x, tsmaller.y), tsmaller.z);
        float tmax = glm::min(glm::min(tbigger.x, tbigger.y), tbigger.z);

        if (tmax >= glm::max(tmin, 0.0f))
        {
            outT = tmin < 0.0f ? tmax : tmin;
            return true;
        }

        return false;
    }
}
