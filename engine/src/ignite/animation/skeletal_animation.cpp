// Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO

#include "skeletal_animation.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <ranges>

namespace ignite
{
    TRS AnimationChannel::CalculateTRS(float timeInTicks, const glm::vec3& defaultTranslation, const glm::quat& defaultRotation, const glm::vec3& defaultScale)
    {
        TRS trs;
        trs.translation = translationKeys.frames.empty()
            ? defaultTranslation : translationKeys.InterpolateTranslation(timeInTicks);

        trs.rotation = rotationKeys.frames.empty()
            ? defaultRotation : rotationKeys.InterpolateRotation(timeInTicks);

        trs.scale = scaleKeys.frames.empty()
            ? defaultScale : scaleKeys.InterpolateScaling(timeInTicks);

        return trs;
    }

    glm::mat4 AnimationChannel::CalculateTransform(float timeInTicks, const glm::vec3 &defaultTranslation, const glm::quat &defaultRotation, const glm::vec3 &defaultScale)
    {
        TRS trs = CalculateTRS(timeInTicks, defaultTranslation, defaultRotation, defaultScale);
        return glm::translate(glm::mat4(1.0f), trs.translation) *
            glm::toMat4(trs.rotation) * glm::scale(glm::mat4(1.0f), trs.scale);
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

    void SkeletalAnimation::SetSkeletonHandle(UUID skeletonHandle)
    {
        m_SkeletonHandle = skeletonHandle;
    }

}
