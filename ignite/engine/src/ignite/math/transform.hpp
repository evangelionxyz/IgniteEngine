// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_TRANSFORM_HPP
#define IGN_TRANSFORM_HPP

#include "ignite/core/base.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>


namespace ignite
{
	struct IGN_API Transform
	{
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;

		Transform()
			: translation(glm::vec3(0.0f))
			, rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
			, scale(glm::vec3(1.0f))
		{
		}

		Transform(const glm::vec3 &tr, const glm::quat &rot, const glm::vec3 &sc)
			: translation(tr), rotation(rot), scale(sc)
		{
		}

		glm::mat4 GetMatrix() const;
		static void Decompose(const glm::mat4 &matrix, Transform &out);
	};
}

#endif