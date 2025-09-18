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

    void ICamera::UpdateMatrices(float aspectRatio)
    {
		view = glm::lookAt(position, target, up);
        switch (projectionType)
        {
            case ProjectionType::Orthographic:
            {
				const float halfH = orthoSize * 0.5f;
				const float halfW = halfH * aspectRatio;
				projection = glm::ortho(-halfW, halfW, -halfH, halfH, nearPlane, farPlane);
                break;
            }
            case ProjectionType::Perspective:
            default:
            {
				projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
                break;
            }
        }
    }

    glm::vec3 ICamera::GetUpDirection() const
    {
		return glm::normalize(glm::cross(GetRightDirection(), GetForwardDirection()));
    }

    glm::vec3 ICamera::GetRightDirection() const
    {
		return glm::normalize(glm::cross(GetForwardDirection(), up));
    }

    glm::vec3 ICamera::GetForwardDirection() const
    {
		return glm::normalize(target - position);
    }
}
