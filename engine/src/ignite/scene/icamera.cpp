// Copyright (c) 2026 Evangelion Manuhutu 

#include "ignite_pch.hpp"

#include "icamera.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"
#include <nvrhi/nvrhi.h>

namespace ignite
{
    ICamera::ICamera()
        : position({0.0f, 0.0f, 0.0f})
    {
    }

	void ICamera::UpdateView()
	{
		switch (projectionType)
		{
		case ProjectionType::Perspective:
			m_View = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(glm::quat({ -pitch, -yaw, 0.0f }));
			m_View = glm::inverse(m_View);
			break;
		case ProjectionType::Orthographic:
			m_View = glm::translate(glm::mat4(1.0f), position);
			m_View = glm::inverse(m_View);
			break;
		}
	}

	void ICamera::UpdateProjection(float width, float height)
    {
        viewportSize = { width, height };
		const float aspectRatio = width / height;
        
        switch (projectionType)
        {
            case ProjectionType::Orthographic:
            {
				const float halfH = orthoSize * 0.5f;
				const float halfW = halfH * aspectRatio;
                m_Projection = glm::orthoZO(-halfW, halfW, -halfH, halfH, nearPlane, farPlane);
                break;
            }
            case ProjectionType::Perspective:
            default:
            {
				m_Projection = glm::perspectiveZO(glm::radians(fov), aspectRatio, nearPlane, farPlane);
                break;
            }
        }
    }

    glm::vec3 ICamera::GetUpDirection() const
    {
        return rotate(glm::quat({ -pitch, -yaw, 0.0f }), { 0.0f, 1.0f, 0.0f });
    }

    glm::vec3 ICamera::GetRightDirection() const
    {
        return rotate(glm::quat({ -pitch, -yaw, 0.0f }), { 1.0f, 0.0f, 0.0f });
    }

    glm::vec3 ICamera::GetForwardDirection() const
    {
        return rotate(glm::quat({ -pitch, -yaw, 0.0f }), { 0.0f, 0.0f, -1.0f });
    }

	glm::mat4 &ICamera::GetProjection()
	{
        return m_Projection;
	}

	glm::mat4 ICamera::GetView()
	{
		return m_View;
	}
}
