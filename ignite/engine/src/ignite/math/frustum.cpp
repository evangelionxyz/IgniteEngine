// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "frustum.hpp"
#include "ignite/scene/icamera.hpp"

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
            m_Planes[(int)Plane::Left][i] = view_projection[i][3] + view_projection[i][0];
            m_Planes[(int)Plane::Right][i] = view_projection[i][3] - view_projection[i][0];
            m_Planes[(int)Plane::Bottom][i] = view_projection[i][3] + view_projection[i][1];
            m_Planes[(int)Plane::Top][i] = view_projection[i][3] - view_projection[i][1];
            m_Planes[(int)Plane::Near][i] = view_projection[i][2];
            m_Planes[(int)Plane::Far][i] = view_projection[i][3] - view_projection[i][2];
        }

        // Normalize the planes
        for (auto &plane : m_Planes)
        {
            float length = glm::length(glm::vec3(plane));
            plane /= length;
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
        for (const auto &plane : m_Planes)
        {
            if (glm::dot(glm::vec3(plane), point) + plane.w < 0)
            {
                return false;
            }
        }
        return true;
    }

    bool Frustum::IsSphereVisible(const glm::vec3 &center, float radius) const
    {
        for (const auto &plane : m_Planes)
        {
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
            {
                return false;
            }
        }

        return true;
    }

    bool Frustum::IsAABBVisible(const glm::vec3 &min, const glm::vec3 &max) const
    {
        const glm::vec3 corners[8] =
        {
            { min.x, min.y, min.z },
            { max.x, min.y, min.z },
            { min.x, max.y, min.z },
            { max.x, max.y, min.z },
            { min.x, min.y, max.z },
            { max.x, min.y, max.z },
            { min.x, max.y, max.z },
            { max.x, max.y, max.z }
        };

        // Test directly against the homogeneous clip volume instead of the
        // extracted world-space planes. This keeps CPU culling aligned with the
        // actual GPU projection rules and avoids backend-specific plane-sign
        // issues when switching between Vulkan and D3D12.
        bool outsideLeft   = true;
        bool outsideRight  = true;
        bool outsideBottom = true;
        bool outsideTop    = true;
        bool outsideNear   = true;
        bool outsideFar    = true;

        for (const glm::vec3 &corner : corners)
        {
            const glm::vec4 clip = m_ViewProjection * glm::vec4(corner, 1.0f);

            outsideLeft   &= (clip.x < -clip.w);
            outsideRight  &= (clip.x >  clip.w);
            outsideBottom &= (clip.y < -clip.w);
            outsideTop    &= (clip.y >  clip.w);
            outsideNear   &= (clip.z <  0.0f);
            outsideFar    &= (clip.z >  clip.w);
        }

        return !(outsideLeft || outsideRight || outsideBottom || outsideTop || outsideNear || outsideFar);
    }

	bool Frustum::IsAABBVisible(const AABB &aabb) const
	{
		return IsAABBVisible(aabb.min, aabb.max);
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
