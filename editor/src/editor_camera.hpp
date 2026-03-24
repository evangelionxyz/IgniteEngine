//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

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
        EditorCamera(const std::string &name);

		void UpdateMouseState();
		void UpdateSphericalPosition();
		void HandleOrbit(float deltaTime);
		void HandlePan(float deltaTime);
		void HandleZoom(float deltaTime);
		void ApplyInertia(float deltaTime);
		void UpdateCameraPosition();

		const std::string& GetName() { return m_Name; }

	private:

		std::string m_Name;
    };
}
