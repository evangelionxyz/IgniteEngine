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

#include "editor_camera.hpp"

namespace ignite
{
    EditorCamera::EditorCamera(std::string name)
        : m_Name(std::move(name))
    {
    }

    void EditorCamera::SetOrbitingTarget(const glm::vec3& target)
    {
        m_OrbitTarget = target;
        UpdateOrbitPosition();
    }

    void EditorCamera::SetOrbitingDistance(float distance)
    {
        m_OrbitDistance = distance;
        UpdateOrbitPosition();
    }

    void EditorCamera::ProcessKeyboardInput(float deltaTime, bool forward, bool backward, bool left, bool right, bool up, bool down)
    {
        float velocity = m_MovementSpeed * deltaTime;

        if (m_MovementMode == MovementMode::Flying)
        {
            if (forward)
                position += GetForwardDirection() * velocity;
            else if (backward)
                position -= GetForwardDirection() * velocity;

            if (left)
                position -= GetRightDirection() * velocity;
            else if (right)
                position += GetRightDirection() * velocity;
            
            if (up)
                position += GetUpDirection() * velocity;
            else if (down)
                position -= GetUpDirection() * velocity;

            UpdateViewMatrix();
        }
        else if (m_MovementMode == MovementMode::Orbiting)
        {
            glm::vec3 forwardDirection = glm::normalize(m_OrbitTarget - position);
            glm::vec3 rightDirection = glm::normalize(glm::cross(forwardDirection, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (forward)
                m_OrbitTarget += forwardDirection * velocity;
            else if (backward)
                m_OrbitTarget -= forwardDirection * velocity;

            if (left)
                m_OrbitTarget += rightDirection * velocity;
            else if (right)
                m_OrbitTarget -= rightDirection * velocity;

            if (up)
                m_OrbitTarget += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
            else if (down)
                m_OrbitTarget -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;

            UpdateOrbitPosition();
        }
    }

    void EditorCamera::ProcessMouseMovement(float deltaTime, float xOffset, float yOffset, bool constrainPitch)
    {
        if (projectionType == ICamera::Type::Perspective)
        {
            float rotationVelocity = m_RotationSpeed * deltaTime;
            xOffset *= rotationVelocity;
            yOffset *= rotationVelocity;

            yaw += xOffset;
            pitch += yOffset;
            pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));

            if (m_MovementMode == MovementMode::Flying)
            {
                UpdateViewMatrix();
            }
            else if (m_MovementMode == MovementMode::Orbiting)
            {
                UpdateOrbitPosition();
            }
        }
    }

    void EditorCamera::ProcessMousePanning(float deltaTime, float xOffset, float yOffset)
    {
        float velocity = m_MovementSpeed * deltaTime;

        if (m_MovementMode == MovementMode::Flying)
        {
            position += GetUpDirection() * yOffset * velocity;
            position += GetRightDirection() * xOffset * velocity;
            UpdateViewMatrix();
        }
        else if (m_MovementMode == MovementMode::Orbiting)
        {
            glm::vec3 forwardDirection = glm::normalize(m_OrbitTarget - position);
            glm::vec3 rightDirection = glm::normalize(glm::cross(forwardDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 upDirection = glm::cross(rightDirection, forwardDirection);

            const float distance = glm::length(m_OrbitTarget - position);
            m_OrbitTarget += upDirection * -yOffset * (distance / m_MaxOrbitDistance) * velocity;
            m_OrbitTarget += rightDirection * xOffset * (distance / m_MaxOrbitDistance) * velocity;

            UpdateOrbitPosition();
        }
    }

    void EditorCamera::ProcessMouseScroll(float yOffset, bool zooming)
    {
        f32 zoomVelocity = m_ZoomSpeed;

        if (projectionType == ICamera::Type::Perspective)
        {
            if (m_MovementMode == MovementMode::Flying || !zooming)
            {
                // In flying mode, scroll wheel changes movement speed
                m_MovementSpeed += yOffset * zoomVelocity;
                m_MovementSpeed = std::max(0.1f, m_MovementSpeed); // Prevent negative or zero speed
            }
            else if (m_MovementMode == MovementMode::Orbiting && zooming)
            {
                // In orbiting mode, scroll wheel changes orbit distance (zoom)
                m_OrbitDistance -= yOffset * zoomVelocity * (m_OrbitDistance / m_MaxOrbitDistance);
                m_OrbitDistance = glm::clamp(m_OrbitDistance, m_MinOrbitDistance, m_MaxOrbitDistance);
            }
        }
        else if (projectionType == ICamera::Type::Orthographic)
        {
            zoom -= yOffset * (zoom / m_MaxOrthoZoom) * 8.0f;
            zoom = glm::clamp(zoom, m_MinOrthoZoom, m_MaxOrthoZoom);

            UpdateProjectionMatrix();
        }

        UpdateOrbitPosition();
    }

    void EditorCamera::UpdateOrbitPosition()
    {
        // Calculate the camera position based on spherical coordinates around the orbit target
        f32 x = m_OrbitTarget.x + m_OrbitDistance * glm::cos(pitch) * glm::cos(yaw);
        f32 y = m_OrbitTarget.y + m_OrbitDistance * glm::sin(pitch);
        f32 z = m_OrbitTarget.z + m_OrbitDistance * glm::cos(pitch) * glm::sin(yaw);

        position = glm::vec3(x, y, z);
       
        // Look at the orbit target
        viewMatrix = glm::lookAt(position, m_OrbitTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void EditorCamera::UpdateFlyingCamera()
    {
        UpdateViewMatrix();
    }

}
