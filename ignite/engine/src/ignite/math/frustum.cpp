// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "frustum.hpp"
#include "ignite/scene/icamera.hpp"

#if defined(__AVX2__) || defined(__AVX__)
#include <immintrin.h>
#endif

namespace ignite {

    Frustum::Frustum(const glm::mat4 &view_projection)
    {
        Update(view_projection);
    }

	Frustum::Frustum(ICamera *camera)
	{
		Update(camera->GetProjection() * camera->GetView());
	}

	void Frustum::Update(const glm::mat4 &view_projection)
    {
        m_ViewProjection = view_projection;

        // Extract frustum planes from the view-projection matrix.
        // The near plane uses the depth-zero-to-one convention that the engine
        // uses for both perspective and orthographic projections.
        for (int i = 0; i < 4; ++i)
        {
            m_Planes[(int)Plane::Left][i]   = view_projection[i][3] + view_projection[i][0];
            m_Planes[(int)Plane::Right][i]  = view_projection[i][3] - view_projection[i][0];
            m_Planes[(int)Plane::Bottom][i] = view_projection[i][3] + view_projection[i][1];
            m_Planes[(int)Plane::Top][i]    = view_projection[i][3] - view_projection[i][1];
            m_Planes[(int)Plane::Near][i]   = view_projection[i][2];
            m_Planes[(int)Plane::Far][i]    = view_projection[i][3] - view_projection[i][2];
        }

        // Normalize the planes
        for (auto &plane : m_Planes)
        {
            float length = glm::length(glm::vec3(plane));
            plane /= length;
        }

        // Populate SOA arrays for AVX2 parallel plane testing (6 active planes, padded to 8 floats)
        for (int i = 0; i < 6; ++i)
        {
            m_PlanesX[i] = m_Planes[i].x;
            m_PlanesY[i] = m_Planes[i].y;
            m_PlanesZ[i] = m_Planes[i].z;
            m_PlanesW[i] = m_Planes[i].w;
        }
        for (int i = 6; i < 8; ++i)
        {
            m_PlanesX[i] = 0.0f;
            m_PlanesY[i] = 0.0f;
            m_PlanesZ[i] = 0.0f;
            m_PlanesW[i] = 0.0f;
        }

        m_ViewProjectionInverse = glm::inverse(view_projection);
        static std::array<glm::vec4, 8> corners =
        {
            glm::vec4{-1, -1, 0, 1},
            glm::vec4{1, -1, 0, 1},
            glm::vec4{1, 1, 0, 1},
            glm::vec4{-1, 1, 0, 1},
            glm::vec4{-1, -1, 1, 1},
            glm::vec4{1, -1, 1, 1},
            glm::vec4{1, 1, 1, 1},
            glm::vec4{-1, 1, 1, 1}
        };

        for (int i = 0; i < 8; ++i)
        {
            glm::vec4 worldSpace = m_ViewProjectionInverse * corners[i];
            m_Corners[i] = glm::vec3(worldSpace) / worldSpace.w;
        }
    }

    bool Frustum::IsPointVisible(const glm::vec3 &point) const
    {
        return IsSphereVisible(point, 0.0f);
    }

    bool Frustum::IsSphereVisible(const glm::vec3 &center, float radius) const
    {
#if defined(__AVX2__) || defined(__AVX__)
        __m256 px = _mm256_load_ps(m_PlanesX);
        __m256 py = _mm256_load_ps(m_PlanesY);
        __m256 pz = _mm256_load_ps(m_PlanesZ);
        __m256 pw = _mm256_load_ps(m_PlanesW);

        __m256 cx = _mm256_set1_ps(center.x);
        __m256 cy = _mm256_set1_ps(center.y);
        __m256 cz = _mm256_set1_ps(center.z);
        __m256 negRadius = _mm256_set1_ps(-radius);

        __m256 dist = _mm256_add_ps(pw,
            _mm256_add_ps(_mm256_mul_ps(px, cx),
            _mm256_add_ps(_mm256_mul_ps(py, cy), _mm256_mul_ps(pz, cz))));

        __m256 outside = _mm256_cmp_ps(dist, negRadius, _CMP_LT_OQ);
        int movemask = _mm256_movemask_ps(outside) & 0x3F;
        return movemask == 0;
#else
        for (const auto &plane : m_Planes)
        {
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
            {
                return false;
            }
        }
        return true;
#endif
    }

    bool Frustum::IsAABBVisible(const glm::vec3 &min, const glm::vec3 &max) const
    {
#if defined(__AVX2__) || defined(__AVX__)
        // Load pre-packed SOA plane equations (8 floats: Left, Right, Bottom, Top, Near, Far, 0, 0)
        __m256 px = _mm256_load_ps(m_PlanesX);
        __m256 py = _mm256_load_ps(m_PlanesY);
        __m256 pz = _mm256_load_ps(m_PlanesZ);
        __m256 pw = _mm256_load_ps(m_PlanesW);

        __m256 vMinX = _mm256_set1_ps(min.x);
        __m256 vMaxX = _mm256_set1_ps(max.x);
        __m256 vMinY = _mm256_set1_ps(min.y);
        __m256 vMaxY = _mm256_set1_ps(max.y);
        __m256 vMinZ = _mm256_set1_ps(min.z);
        __m256 vMaxZ = _mm256_set1_ps(max.z);

        // Select positive vertex for each plane normal direction:
        // If plane normal component >= 0, use max, else min.
        __m256 maskX = _mm256_cmp_ps(px, _mm256_setzero_ps(), _CMP_GE_OQ);
        __m256 maskY = _mm256_cmp_ps(py, _mm256_setzero_ps(), _CMP_GE_OQ);
        __m256 maskZ = _mm256_cmp_ps(pz, _mm256_setzero_ps(), _CMP_GE_OQ);

        __m256 vx = _mm256_blendv_ps(vMinX, vMaxX, maskX);
        __m256 vy = _mm256_blendv_ps(vMinY, vMaxY, maskY);
        __m256 vz = _mm256_blendv_ps(vMinZ, vMaxZ, maskZ);

        // Evaluate plane distance for all 6 planes simultaneously:
        // dist = px * vx + py * vy + pz * vz + pw
        __m256 dist = _mm256_add_ps(pw,
            _mm256_add_ps(_mm256_mul_ps(px, vx),
            _mm256_add_ps(_mm256_mul_ps(py, vy), _mm256_mul_ps(pz, vz))));

        // If dist < 0 for any of the 6 active frustum planes, the AABB is outside
        __m256 outside = _mm256_cmp_ps(dist, _mm256_setzero_ps(), _CMP_LT_OQ);
        int movemask = _mm256_movemask_ps(outside) & 0x3F;
        return movemask == 0;
#else
        for (const auto &plane : m_Planes)
        {
            glm::vec3 p(
                plane.x >= 0.0f ? max.x : min.x,
                plane.y >= 0.0f ? max.y : min.y,
                plane.z >= 0.0f ? max.z : min.z
            );
            if (glm::dot(glm::vec3(plane), p) + plane.w < 0.0f)
            {
                return false;
            }
        }
        return true;
#endif
    }

	bool Frustum::IsAABBVisible(const AABB &aabb) const
	{
		return IsAABBVisible(aabb.min, aabb.max);
	}

    void Frustum::IsAABBVisibleBatch(const AABB *aabbs, size_t count, uint8_t *outVisibilityResults) const
    {
        for (size_t i = 0; i < count; ++i)
        {
            outVisibilityResults[i] = IsAABBVisible(aabbs[i].min, aabbs[i].max) ? 1 : 0;
        }
    }

	std::vector<std::pair<glm::vec3, glm::vec3>> Frustum::GetEdges() const
    {
        return
        {
            {m_Corners[0], m_Corners[1]}, {m_Corners[1], m_Corners[2]}, {m_Corners[2], m_Corners[3]}, {m_Corners[3], m_Corners[0]},
            {m_Corners[4], m_Corners[5]}, {m_Corners[5], m_Corners[6]}, {m_Corners[6], m_Corners[7]}, {m_Corners[7], m_Corners[4]},
            {m_Corners[0], m_Corners[4]}, {m_Corners[1], m_Corners[5]}, {m_Corners[2], m_Corners[6]}, {m_Corners[3], m_Corners[7]}
        };
    }
}
