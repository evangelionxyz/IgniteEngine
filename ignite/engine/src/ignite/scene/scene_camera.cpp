// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "scene_camera.hpp"

namespace ignite
{
    void SceneCamera::SetTransform(const glm::mat4 &transform)
	{
		m_Transform = transform;
		position = glm::vec3(transform[3]);
	}

	glm::mat4 SceneCamera::GetView()
	{
      return glm::inverse(m_Transform);
	}

    void SceneCamera::SetAspectRatioPreset(AspectRatioPreset preset)
    {
		m_AspectRatioPreset = preset;
		if (preset == AspectRatioPreset::Free)
			return;

		const float aspect = m_ViewportSize.x / m_ViewportSize.y;
		const float targetAspect = GetAspectRatioValue();

        if (aspect > targetAspect)
        {
			m_ViewportSize.x = m_ViewportSize.y * targetAspect;
        }
        else
        {
			m_ViewportSize.y = m_ViewportSize.x / targetAspect;
        }
    }

    float SceneCamera::GetAspectRatioValue() const
	{
		switch (m_AspectRatioPreset)
		{
		case AspectRatioPreset::Ratio16x9: return 16.0f / 9.0f;
		case AspectRatioPreset::Ratio16x10: return 16.0f / 10.0f;
		case AspectRatioPreset::Ratio4x3: return 4.0f / 3.0f;
		case AspectRatioPreset::Ratio21x9: return 21.0f / 9.0f;
		case AspectRatioPreset::Ratio1x1: return 1.0f;
		case AspectRatioPreset::Free:
		default:
			return 0.0f;
		}
	}
}
