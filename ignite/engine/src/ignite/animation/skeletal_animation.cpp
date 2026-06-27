// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "skeletal_animation.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <ranges>

namespace ignite
{
    Transform AnimationChannel::Calculate(float timeInTicks, const Transform& defaultTransform)
    {
        Transform tr;
        tr.translation = translationKeys.frames.empty()
            ? defaultTransform.translation : translationKeys.InterpolateTranslation(timeInTicks);

        tr.rotation = rotationKeys.frames.empty()
            ? defaultTransform.rotation : rotationKeys.InterpolateRotation(timeInTicks);

        tr.scale = scaleKeys.frames.empty()
            ? defaultTransform.scale : scaleKeys.InterpolateScaling(timeInTicks);

        return tr;
    }

    bool SkeletalAnimation::Serialize(const ignite::Path &filepath)
	{
		BinarySerializer::SerializeSkeletalAnimation(this, filepath);
        SetDirtyFlag(false);
		return true;
	}

	Ref<SkeletalAnimation> SkeletalAnimation::Deserialize(const ignite::Path &filepath)
	{
        return BinarySerializer::DeserializeSkeletalAnimation(filepath);
	}

    void SkeletalAnimation::SetSkeletonHandle(UUID skeletonHandle)
    {
        m_SkeletonHandle = skeletonHandle;
    }

}
