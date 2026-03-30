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

#include "icamera.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"

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
        this->width = width;
        this->height = height;

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
