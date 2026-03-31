//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once

#include "ignite/scene/icamera.hpp"

namespace ignite
{
    class EditorCamera : public ICamera
    {
    public:

        enum class NavigationMode
        {
            Orbit,
			Fly,
			Mode2D
        };

        EditorCamera() = default;
        EditorCamera(const std::string &name);

		void SetView(const glm::mat4 &view);

		void UpdateMouseState();
		void UpdateSphericalPosition();
		void HandleOrbit(float deltaTime);
        void HandleFly(float deltaTime);
		void HandlePan(float deltaTime);
		void HandleZoom(float deltaTime);
		void ApplyInertia(float deltaTime);
		void UpdateCameraPosition();

		void SetNavigationMode(NavigationMode mode);
		NavigationMode GetNavigationMode() const { return m_NavigationMode; }

		void SetPanSnapValue(float snap) { m_PanSnapValue = snap; }
		float GetPanSnapValue() const { return m_PanSnapValue; }

		virtual void UpdateView() override;
		virtual void UpdateProjection(float width, float height) override;

		void SetDistance(float distance) { m_Distance = distance; }
		float GetDistance() { return m_Distance; }

		void SetTarget(const glm::vec3 &target) { m_Target = target; }
		const glm::vec3 &GetTarget() { return m_Target; }

		virtual glm::vec3 GetUpDirection() const override;
		virtual glm::vec3 GetRightDirection() const override;
		virtual glm::vec3 GetForwardDirection() const override;

		const std::string& GetName() { return m_Name; }

	private:
		glm::vec2 m_AngularVelocity = glm::vec2(0.0f);
		glm::vec2 m_PanVelocity = glm::vec2(0.0f);
		float m_ZoomVelocity = 0.0f;
        float m_Distance = 1.0f;
		float m_PanSnapValue = 0.0f;
		float m_FlySpeed = 6.0f;

		glm::vec3 m_Target = { 0.0f, 0.0f, -1.0f };
		NavigationMode m_NavigationMode = NavigationMode::Orbit;
		
		std::string m_Name;
    };
}
