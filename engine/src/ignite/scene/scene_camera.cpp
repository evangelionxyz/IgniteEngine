// Copyright (c) 2026 Evangelion Manuhutu

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
}
