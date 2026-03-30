//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "editor_camera.hpp"

#include "ignite/core/application.hpp"

namespace ignite
{
    EditorCamera::EditorCamera(const std::string &name)
        : m_Name(name)
    {
    }

	void EditorCamera::SetView(const glm::mat4 &view)
	{
		m_View = view;
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
		position.x = m_Target.x + m_Distance * cos(pitch) * cos(yaw);
		position.y = m_Target.y + m_Distance * sin(pitch);
		position.z = m_Target.z + m_Distance * cos(pitch) * sin(yaw);
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
						m_ZoomVelocity += delta.y;
					}
					else
					{
						// direct zoom for imediate response
						m_Distance -= delta.y;
						m_Distance = glm::clamp(m_Distance, controls.minDistance, controls.maxDistance);
					}
				}

				// apply zoom velocity
				if (controls.enableInertia && abs(m_ZoomVelocity) > 0.001f)
				{
					m_Distance -= m_ZoomVelocity * deltaTime;
					m_Distance = glm::clamp(m_Distance, controls.minDistance, controls.maxDistance);
					
					// dampen zoom velocity
					m_ZoomVelocity *= controls.zoomDamping;

					// stop very small velocities
					if (abs(m_ZoomVelocity) < 0.001f)
					{
						m_ZoomVelocity = 0.0f;
					}
				}
			}
			else // handle orbit
			{
				// apply mouse movement to angular velocity for inertia
				if (controls.enableInertia)
				{
					m_AngularVelocity.x += delta.x * controls.mouseSensitivity;
					m_AngularVelocity.y += delta.y * controls.mouseSensitivity;
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
			const glm::vec3 rightVector = GetRightDirection();
			const glm::vec3 upVector = GetUpDirection();

			// pan in the camera's right vector and up vector
			const float panSpeed = controls.panSensitivity * m_Distance;
			const glm::vec3 panVector = rightVector * (-delta.x * panSpeed) + upVector * (delta.y * panSpeed);

			// apply pan to target
			m_Target += panVector;
			if (controls.enableInertia)
			{
				m_PanVelocity = delta * controls.panSensitivity;
			}
		}
	}

	void EditorCamera::HandleZoom(float deltaTime)
	{
		float wheelDelta = 0.0f;

		// check for scroll wheel input
		if (mouse.scroll.y != 0)
		{
			wheelDelta = static_cast<float>(mouse.scroll.y);

			// reset scroll after processing
			mouse.scroll.y = 0;
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
				m_ZoomVelocity += wheelDelta * controls.zoomSensitivity;
			}
			else
			{
				if (projectionType == ProjectionType::Perspective)
				{
					// Direct zoom for immediate response
					m_Distance -= wheelDelta * controls.zoomSensitivity;
					m_Distance = glm::clamp(m_Distance, controls.minDistance, controls.maxDistance);
				}
				else if (projectionType == ProjectionType::Orthographic)
				{
					orthoSize -= wheelDelta * controls.zoomSensitivity * 0.5f;
					orthoSize = glm::clamp(orthoSize, controls.minOrthoSize, controls.maxOrthoSize);

					if (this->width > 0.0f && this->height > 0.0f)
					{
						UpdateView();
						UpdateProjection(this->width, this->height);
					}
				}
			}
		}

		// Apply zoom velocity
		if (controls.enableInertia && abs(m_ZoomVelocity) > 0.001f)
		{
			if (projectionType == ProjectionType::Perspective)
			{
				m_Distance -= m_ZoomVelocity * deltaTime * 10.0f;
				m_Distance = glm::clamp(m_Distance, controls.minDistance, controls.maxDistance);
			}
			else if (projectionType == ProjectionType::Orthographic)
			{
				orthoSize -= wheelDelta * controls.zoomSensitivity * 0.5f;
				orthoSize = glm::clamp(orthoSize, controls.minOrthoSize, controls.maxOrthoSize);
				
				if (this->width > 0.0f && this->height > 0.0f)
				{
					UpdateView();
					UpdateProjection(this->width, this->height);
				}
			}

			// Dampen zoom velocity
			m_ZoomVelocity *= controls.zoomDamping;

			// Stop very small velocities
			if (abs(m_ZoomVelocity) < 0.001f)
			{
				m_ZoomVelocity = 0.0f;
			}
		}
	}

	void EditorCamera::ApplyInertia(float deltaTime)
	{
		// apply angular intertia
		if (glm::length(m_AngularVelocity) > 0.001f)
		{
			yaw += m_AngularVelocity.x * deltaTime;
			pitch += m_AngularVelocity.y * deltaTime;
			pitch = glm::clamp(pitch, controls.minPitch, controls.maxPitch);

			// dampen angular velocity
			m_AngularVelocity *= controls.inertiaDamping;

			// stop very small velocities
			if (glm::length(m_AngularVelocity) < 0.001f)
			{
				m_AngularVelocity = glm::vec2(0.0f);
			}
		}

		// apply pan inertia
		if (glm::length(m_PanVelocity) > 0.001f)
		{
			const glm::vec3 right = GetRightDirection();
			const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

			const float panSpeed = m_Distance;
			const glm::vec3 panVector = right * (-m_PanVelocity.x * panSpeed) + worldUp * (m_PanVelocity.y * panSpeed);

			m_Target += panVector * deltaTime;

			// dampen pan velocity
			m_PanVelocity *= controls.inertiaDamping;

			// stop very small velocities
			if (glm::length(m_PanVelocity) < 0.001f)
			{
				m_PanVelocity = glm::vec2(0.0f);
			}
		}
	}

	void EditorCamera::UpdateCameraPosition()
	{
		UpdateSphericalPosition();
	}

	void EditorCamera::UpdateView()
	{
		m_View = glm::lookAt(position, m_Target, { 0.0f, 1.0f, 0.0f });
	}

	void EditorCamera::UpdateProjection(float width, float height)
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

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::normalize(glm::cross(GetRightDirection(), GetForwardDirection()));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::normalize(glm::cross(GetForwardDirection(), { 0.0f, 1.0f, 0.0f }));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::normalize(m_Target - position);
	}

}
