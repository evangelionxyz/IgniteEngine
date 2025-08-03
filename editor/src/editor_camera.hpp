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

#pragma once

#include "ignite/scene/icamera.hpp"

namespace ignite
{
    class EditorCamera : public ICamera
    {
    public:

        enum class MovementMode
        {
            Orbiting,
            Flying
        };

        EditorCamera() = default;
        EditorCamera(std::string name);

        // Camera movement mode control
        void SetMovementMode(MovementMode mode) { m_MovementMode = mode; }

        // Orbiting mode controls
        void SetOrbitingTarget(const glm::vec3& target);
        void SetOrbitingDistance(float distance);

        // Movement input handling
        void ProcessKeyboardInput(float deltaTime, bool forward, bool backward, bool left, bool right, bool up, bool down);
        void ProcessMouseMovement(float deltaTime, float xOffset, float yOffset, bool constrainPitch = true);
        void ProcessMousePanning(float deltaTime, float xOffset, float yOffset);
        void ProcessMouseScroll(float yOffset, bool zooming);

        // Movement input handling
        void SetMovementSpeed(float speed) { m_MovementSpeed = speed; }
        void SetRotationSpeed(float speed) { m_RotationSpeed = speed; }
        void SetZoomSpeed(float speed) { m_ZoomSpeed = speed; }

        // Getters for camera properties
        MovementMode GetMovementMode() const { return m_MovementMode; }
        glm::vec3 GetOrbitingTarget() const { return m_OrbitTarget; }
        float GetMovementSpeed() const { return m_MovementSpeed; }
        float GetRotationSpeed() const { return m_RotationSpeed; }
        float GetZoomSpeed() const { return m_ZoomSpeed; }
        float GetOrbitDistance() const { return m_OrbitDistance; }
        float GetMinOrbitDistance() const { return m_MinOrbitDistance; }
        float GetMaxOrbitDistance() const { return m_MaxOrbitDistance; }

        void UpdateOrbitPosition();
        void UpdateFlyingCamera();
    private:
        std::string m_Name;
        MovementMode m_MovementMode = MovementMode::Orbiting;
        glm::vec3 m_OrbitTarget = { 0.0f, 0.0f, 0.0f }; // Target point for orbiting camera
        float m_OrbitDistance = 10.0f; // Distance from the target point in orbiting mode

        // Movement settings
        float m_MovementSpeed = 5.0f; // Speed of camera movement
        float m_RotationSpeed = 0.25f; // Speed of camera rotation
        float m_ZoomSpeed = 5.0f; // Speed of camera zooming

        // Flying mode constraints
        float m_MinOrbitDistance = 1.0f; // Minimum distance for orbiting
        float m_MaxOrbitDistance = 100.0f; // Maximum distance for orbiting
    };
}
