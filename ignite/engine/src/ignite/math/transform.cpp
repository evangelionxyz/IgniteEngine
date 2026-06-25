#include "transform.hpp"

namespace ignite
{
	glm::mat4 Transform::GetMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), translation) 
			* glm::toMat4(rotation) 
			* glm::scale(glm::mat4(1.0f), scale);
	}

	void Transform::Decompose(const glm::mat4 &matrix, Transform &out)
	{
		static glm::vec3 skew;
		static glm::vec4 perspective;

		if (!glm::decompose(matrix, out.scale, out.rotation, out.translation, skew, perspective))
		{
			out.translation = glm::vec3(0.0f);
			out.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			out.scale = glm::vec3(1.0f);
		}

		out.rotation = glm::normalize(out.rotation);
	}
}