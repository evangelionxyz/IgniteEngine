// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "aabb.hpp"

#include "ignite/graphics/objects/mesh.hpp"

namespace ignite
{
    AABB AABB::CalculateMeshAABB(const std::vector<Ref<MeshInstance>> &meshInstances)
    {
        AABB bounds;

        for (const Ref<MeshInstance> &mesh : meshInstances)
        {
            if (!mesh)
                continue;

            auto &prim = mesh->GetPrimitive();
            if (!prim || prim->vertices.empty())
            {
                continue;
            }

            AABB meshBounds;
            // Vertices are already transformed when the mesh is loaded, so
            // use vertex positions directly and do not apply the mesh local transform.
            for (const VertexMesh_Anim &vertex : prim->vertices)
            {
                meshBounds.min = glm::min(meshBounds.min, vertex.position);
                meshBounds.max = glm::max(meshBounds.max, vertex.position);
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
