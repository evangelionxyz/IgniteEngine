// Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO

#include "skeletal_animation.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ignite {

    glm::mat4 AnimationChannel::CalculateTransform(float timeInTicks, const glm::vec3& defaultTranslation, const glm::quat& defaultRotation, const glm::vec3& defaultScale)
    {
        translation = translationKeys.frames.empty()
            ? defaultTranslation
            : translationKeys.InterpolateTranslation(timeInTicks);

        rotation = rotationKeys.frames.empty()
            ? defaultRotation
            : rotationKeys.InterpolateRotation(timeInTicks);

        scale = scaleKeys.frames.empty()
            ? defaultScale
            : scaleKeys.InterpolateScaling(timeInTicks);

        return glm::translate(glm::mat4(1.0f), translation)
            * glm::toMat4(rotation)
            * glm::scale(glm::mat4(1.0f), scale);
    }

	bool SkeletalAnimation::Serialize(const std::filesystem::path &filepath)
	{
		BinarySerializer::SerializeSkeletalAnimation(this, filepath);
		return true;
	}

	Ref<SkeletalAnimation> SkeletalAnimation::Deserialize(const std::filesystem::path &filepath)
	{
        return BinarySerializer::DeserializeSkeletalAnimation(filepath);
	}
}
