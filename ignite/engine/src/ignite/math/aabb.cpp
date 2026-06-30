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

    AABB AABB::Transform(const glm::mat4 &M) const
    {
        glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, max.y, max.z)
        };

        AABB result;
        result.min = glm::vec3(M * glm::vec4(corners[0], 1.0f));
        result.max = result.min;

        for (int i = 1; i < 8; ++i)
        {
            glm::vec3 transformed = glm::vec3(M * glm::vec4(corners[i], 1.0f));
            result.min = glm::min(result.min, transformed);
            result.max = glm::max(result.max, transformed);
        }
        return result;
    }
}
