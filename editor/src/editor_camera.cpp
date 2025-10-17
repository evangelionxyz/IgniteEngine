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

#include "ignite/core/application.hpp"

namespace ignite
{
    EditorCamera::EditorCamera(const std::string &name)
        : m_Name(name)
    {
    }

	void EditorCamera::UpdateMouseState()
	{
		// store last position before updating
		mouse.lastPosition = mouse.position;

		// update current position
		mouse.position = Input::GetMousePosition();

		// update button states
		mouse.leftButtonDown = Input::IsMouseButtonPressed(Mouse::ButtonLeft);
		mouse.middleButtonDown = Input::IsMouseButtonPressed(Mouse::ButtonMiddle);
		mouse.rightButtonDown = Input::IsMouseButtonPressed(Mouse::ButtonRight);
	}

	void EditorCamera::UpdateSphericalPosition()
	{
		position.x = target.x + distance * cos(pitch) * cos(yaw);
		position.y = target.y + distance * sin(pitch);
		position.z = target.z + distance * cos(pitch) * sin(yaw);
	}

	void EditorCamera::HandleOrbit(float deltaTime)
	{
		// auto window = Application::GetInstance()->GetWindow()->GetWindowHandle();

		if (mouse.rightButtonDown)
		{
			glm::vec2 delta = mouse.position - mouse.lastPosition;

			// handle zoom
			if (Input::IsModifierPressed(KeyMod::LeftControl))
			{
				delta.y *= -1.0f * 0.5f; // inverting mouse y

				if (delta.y != 0.0f)
				{
					// apply zoom velocity for smooth zooming
					if (controls.enableInertia)
					{
						zoomVelocity += delta.y;
					}
					else
					{
						// direct zoom for imediate response
						distance -= delta.y;
						distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
					}
				}

				// apply zoom velocity
				if (controls.enableInertia && abs(zoomVelocity) > 0.001f)
				{
					distance -= zoomVelocity * deltaTime;
					distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
					
					// dampen zoom velocity
					zoomVelocity *= controls.zoomDamping;

					// stop very small velocities
					if (abs(zoomVelocity) < 0.001f)
					{
						zoomVelocity = 0.0f;
					}
				}
			}
			else // handle orbit
			{
				// apply mouse movement to angular velocity for inertia
				if (controls.enableInertia)
				{
					angularVelocity.x += delta.x * controls.mouseSensitivity;
					angularVelocity.y += delta.y * controls.mouseSensitivity;
				}

				// directly update angles for immediate response
				yaw += delta.x * controls.mouseSensitivity;
				pitch += delta.y * controls.mouseSensitivity;

				// Clamp pitch to prevent camera flipping
				pitch = glm::clamp(pitch, controls.minPitch, controls.maxPitch);
			}
		}
	}

	void EditorCamera::HandlePan(float deltaTime)
	{
		if (mouse.middleButtonDown)
		{
			const glm::vec2 delta = mouse.position - mouse.lastPosition;

			// calculate pan direction in camera space
			const glm::vec3 right = GetRightDirection();
			const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

			// pan in the camera's right and world up directions
			const float panSpeed = controls.panSensitivity * distance;
			const glm::vec3 panVector = right * (-delta.x * panSpeed) + worldUp * (delta.y * panSpeed);

			// apply pan to target
			target += panVector;
			if (controls.enableInertia)
			{
				panVelocity = delta * controls.panSensitivity;
			}
		}
	}

	void EditorCamera::HandleZoom(float deltaTime)
	{
		float wheelDelta = 0.0f;

		// check for scroll wheel input
		if (mouse.scroll.y != 0.0f)
		{
			wheelDelta = mouse.scroll.y;

			// reset scroll after processing
			mouse.scroll.y = 0.0f;
		}

		// handle keyboard zoom controls
		if (Input::IsKeyPressed(Key::Equal) || Input::IsKeyPressed(Key::KPAdd))
		{
			wheelDelta -= controls.zoomSensitivity * deltaTime * 10.0f;
		}
		if (Input::IsKeyPressed(Key::Minus) || Input::IsKeyPressed(Key::KPSubtract))
		{
			wheelDelta += controls.zoomSensitivity * deltaTime * 10.0f;
		}

		if (wheelDelta != 0.0f)
		{
			// Apply zoom velocity for smooth zooming
			if (controls.enableInertia)
			{
				zoomVelocity += wheelDelta * controls.zoomSensitivity;
			}
			else
			{
				// Direct zoom for immediate response
				distance -= wheelDelta * controls.zoomSensitivity;
				distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
			}
		}

		// Apply zoom velocity
		if (controls.enableInertia && abs(zoomVelocity) > 0.001f)
		{
			distance -= zoomVelocity * deltaTime * 10.0f;
			distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);

			// Dampen zoom velocity
			zoomVelocity *= controls.zoomDamping;

			// Stop very small velocities
			if (abs(zoomVelocity) < 0.001f)
			{
				zoomVelocity = 0.0f;
			}
		}
	}

	void EditorCamera::ApplyInertia(float deltaTime)
	{
		// apply angular intertia
		if (glm::length(angularVelocity) > 0.001f)
		{
			yaw += angularVelocity.x * deltaTime;
			pitch += angularVelocity.y * deltaTime;
			pitch = glm::clamp(pitch, controls.minPitch, controls.maxPitch);

			// dampen angular velocity
			angularVelocity *= controls.inertiaDamping;

			// stop very small velocities
			if (glm::length(angularVelocity) < 0.001f)
			{
				angularVelocity = glm::vec2(0.0f);
			}
		}

		// apply pan inertia
		if (glm::length(panVelocity) > 0.001f)
		{
			const glm::vec3 right = GetRightDirection();
			const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

			const float panSpeed = distance;
			const glm::vec3 panVector = right * (-panVelocity.x * panSpeed) + worldUp * (panVelocity.y * panSpeed);

			target += panVector * deltaTime;

			// dampen pan velocity
			panVelocity *= controls.inertiaDamping;

			// stop very small velocities
			if (glm::length(panVelocity) < 0.001f)
			{
				panVelocity = glm::vec2(0.0f);
			}
		}
	}

	void EditorCamera::UpdateCameraPosition()
	{
		// update camera position based on spherical coordinates
		UpdateSphericalPosition();

		view = glm::lookAt(position, target, { 0.0f, 1.0f, 0.0f });
	}
}
