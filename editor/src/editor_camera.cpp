//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "editor_camera.hpp"

#include "ignite/core/application.hpp"

#include <cmath>

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

	void EditorCamera::FocusTarget(const glm::vec3 &target, float distance)
	{
		m_FocusTarget = target;
		m_FocusDistance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
		m_FocusActive = true;
	}

	void EditorCamera::SetNavigationMode(NavigationMode mode)
	{
		if (m_NavigationMode == mode)
			return;

		const NavigationMode previousMode = m_NavigationMode;
		glm::vec3 previousForward = m_Target - position;
		if (glm::length(previousForward) < 0.0001f)
		{
			previousForward = { 0.0f, 0.0f, -1.0f };
		}
		else
		{
			previousForward = glm::normalize(previousForward);
		}

		m_NavigationMode = mode;

		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			projectionType = ProjectionType::Orthographic;
			m_Target.z = 0.0f;
			m_Distance = glm::clamp(m_Distance, controls.minDistance, controls.maxDistance);
			position = { m_Target.x, m_Target.y, m_Distance };
			pitch = 0.0f;
			yaw = 0.0f;
		}
		else
		{
			if (projectionType == ProjectionType::Orthographic)
			{
				projectionType = ProjectionType::Perspective;
			}

			if (previousMode == NavigationMode::Fly && m_NavigationMode == NavigationMode::Orbit)
			{
				m_Distance = glm::clamp(m_Distance, controls.minDistance, controls.maxDistance);
				const glm::vec3 forward = GetForwardDirection();
				m_Target = position + forward * m_Distance;
			}
			else if (previousMode == NavigationMode::Orbit && m_NavigationMode == NavigationMode::Fly)
			{
				const glm::vec3 forward = glm::normalize(previousForward);
				pitch = glm::clamp(std::asin(glm::clamp(-forward.y, -1.0f, 1.0f)), controls.minPitch, controls.maxPitch);
				yaw = std::atan2(-forward.z, -forward.x);
				m_Distance = glm::clamp(glm::distance(position, m_Target), controls.minDistance, controls.maxDistance);
				m_Target = position + GetForwardDirection() * m_Distance;
			}
			else
			{
				m_Distance = glm::clamp(glm::distance(position, m_Target), controls.minDistance, controls.maxDistance);
			}
		}
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
		if (m_NavigationMode != NavigationMode::Orbit)
		{
			return;
		}

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

	void EditorCamera::HandleFly(float deltaTime)
	{
		if (m_NavigationMode != NavigationMode::Fly)
			return;

		if (mouse.rightButtonDown)
		{
			const glm::vec2 delta = mouse.position - mouse.lastPosition;
			yaw += delta.x * controls.mouseSensitivity;
			pitch += delta.y * controls.mouseSensitivity;
			pitch = glm::clamp(pitch, controls.minPitch, controls.maxPitch);
		}

		glm::vec3 moveDir(0.0f);
		if (Input::IsKeyPressed(Key::W)) moveDir += GetForwardDirection();
		if (Input::IsKeyPressed(Key::S)) moveDir -= GetForwardDirection();
		if (Input::IsKeyPressed(Key::D)) moveDir += GetRightDirection();
		if (Input::IsKeyPressed(Key::A)) moveDir -= GetRightDirection();

		if (glm::length(moveDir) > 0.0f)
		{
			float speed = m_FlySpeed;
			if (Input::IsModifierPressed(KeyMod::LeftShift) || Input::IsModifierPressed(KeyMod::RightShift))
				speed *= 2.5f;

			if (Input::IsModifierPressed(KeyMod::LeftControl) || Input::IsModifierPressed(KeyMod::RightControl))
				speed *= 0.35f;

			position += glm::normalize(moveDir) * speed * deltaTime;
		}

		m_Target = position + GetForwardDirection() * glm::max(m_Distance, 1.0f);
	}

	void EditorCamera::HandlePan(float deltaTime)
	{
		(void)deltaTime;

		if (!mouse.middleButtonDown && !(m_NavigationMode == NavigationMode::Mode2D && mouse.rightButtonDown))
		{
			return;
		}

		const glm::vec2 delta = mouse.position - mouse.lastPosition;
		if (delta.x == 0.0f && delta.y == 0.0f)
		{
			return;
		}

		const float safeHeight = glm::max(height, 1.0f);
		float panUnitsPerPixel = controls.panSensitivity;

		if (projectionType == ProjectionType::Orthographic)
		{
			const float minOrthoSize = 0.1f;
			orthoSize = glm::max(orthoSize, minOrthoSize);
			panUnitsPerPixel = orthoSize / safeHeight;
		}
		else
		{
			const float distanceScale = glm::max(m_Distance, controls.minDistance);
			const float worldHeight = 2.0f * std::tan(glm::radians(fov) * 0.5f) * distanceScale;
			panUnitsPerPixel = worldHeight / safeHeight;
		}

		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			m_Target.x += -delta.x * panUnitsPerPixel;
			m_Target.y += delta.y * panUnitsPerPixel;

			if (m_PanSnapValue > 0.0f)
			{
				m_Target.x = std::round(m_Target.x / m_PanSnapValue) * m_PanSnapValue;
				m_Target.y = std::round(m_Target.y / m_PanSnapValue) * m_PanSnapValue;
			}

			return;
		}

		const glm::vec3 rightVector = GetRightDirection();
		const glm::vec3 upVector = GetUpDirection();
		const glm::vec3 panVector = rightVector * (-delta.x * panUnitsPerPixel) + upVector * (delta.y * panUnitsPerPixel);

		if (m_NavigationMode == NavigationMode::Fly)
		{
			position += panVector;
			m_Target += panVector;
			return;
		}

		if (m_NavigationMode == NavigationMode::Orbit)
		{
			m_Target += panVector;
			if (controls.enableInertia && projectionType == ProjectionType::Perspective)
			{
				m_PanVelocity = delta * panUnitsPerPixel;
			}
		}
	}

	void EditorCamera::HandleZoom(float deltaTime)
	{
		float wheelDelta = 0.0f;

		// check for scroll wheel input
		if (mouse.scroll.y != 0)
		{
			m_FocusActive = false;

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
			if (m_NavigationMode == NavigationMode::Fly)
			{
				position += GetForwardDirection() * (wheelDelta * controls.zoomSensitivity);
				m_Target = position + GetForwardDirection() * glm::max(m_Distance, 1.0f);
				return;
			}

			if (projectionType == ProjectionType::Orthographic)
			{
				const float minOrthoSize = 0.1f;
				const float zoomStep = glm::max(orthoSize * 0.12f, 0.01f);
				orthoSize -= wheelDelta * controls.zoomSensitivity * zoomStep;
				orthoSize = glm::clamp(orthoSize, minOrthoSize, controls.maxOrthoSize);
				m_Distance = glm::clamp(orthoSize, controls.minDistance, controls.maxDistance);
				m_ZoomVelocity = 0.0f;

				if (this->width > 0.0f && this->height > 0.0f)
				{
					UpdateView();
					UpdateProjection(this->width, this->height);
				}

				return;
			}

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
					const float minOrthoSize = 0.1f;
					const float zoomStep = glm::max(orthoSize * 0.12f, 0.01f);
					orthoSize -= wheelDelta * controls.zoomSensitivity * zoomStep;
					orthoSize = glm::clamp(orthoSize, minOrthoSize, controls.maxOrthoSize);
					m_Distance = glm::clamp(orthoSize, controls.minDistance, controls.maxDistance);

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
				m_ZoomVelocity = 0.0f;
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
		if (projectionType == ProjectionType::Orthographic)
		{
			m_AngularVelocity = glm::vec2(0.0f);
			m_PanVelocity = glm::vec2(0.0f);
			m_ZoomVelocity = 0.0f;
			return;
		}

		if (m_NavigationMode != NavigationMode::Orbit)
		{
			return;
		}

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

	void EditorCamera::UpdateCameraPosition(float deltaTime)
	{
	   if (m_FocusActive)
	   {
		   const float blend = 1.0f - std::exp(-m_FocusSpeed * glm::max(deltaTime, 0.0f));
		   m_Target = glm::mix(m_Target, m_FocusTarget, blend);
		   m_Distance = glm::mix(m_Distance, m_FocusDistance, blend);

		   if (glm::length(m_Target - m_FocusTarget) < 0.001f && std::abs(m_Distance - m_FocusDistance) < 0.001f)
		   {
			   m_Target = m_FocusTarget;
			   m_Distance = m_FocusDistance;
			   m_FocusActive = false;
		   }
	   }

		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			position = { m_Target.x, m_Target.y, m_Distance };
			return;
		}

		if (m_NavigationMode == NavigationMode::Fly)
		{
			m_Target = position + GetForwardDirection() * glm::max(m_Distance, 1.0f);
			return;
		}

		UpdateSphericalPosition();
	}

	void EditorCamera::UpdateView()
	{
		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			const glm::vec3 target = { m_Target.x, m_Target.y, 0.0f };
			m_View = glm::lookAt(position, target, { 0.0f, 1.0f, 0.0f });
			return;
		}

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
		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			return { 0.0f, 1.0f, 0.0f };
		}

		return glm::normalize(glm::cross(GetRightDirection(), GetForwardDirection()));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			return { 1.0f, 0.0f, 0.0f };
		}

		return glm::normalize(glm::cross(GetForwardDirection(), { 0.0f, 1.0f, 0.0f }));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		if (m_NavigationMode == NavigationMode::Mode2D)
		{
			return { 0.0f, 0.0f, -1.0f };
		}

		if (m_NavigationMode == NavigationMode::Fly)
		{
			const float cp = std::cos(pitch);
			const float sp = std::sin(pitch);
			const float cy = std::cos(yaw);
			const float sy = std::sin(yaw);
			return glm::normalize(glm::vec3(-cp * cy, -sp, -cp * sy));
		}

		return glm::normalize(m_Target - position);
	}
}