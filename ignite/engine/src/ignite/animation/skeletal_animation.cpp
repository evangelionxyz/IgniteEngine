// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "skeletal_animation.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

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

	SkeletalAnimation::~SkeletalAnimation()
	{
		if (m_SkeletonHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_SkeletonHandle,
				std::format("skeletal-animtion.{}.{}", static_cast<uint64_t>(handle), static_cast<uint64_t>(m_SkeletonHandle)));
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

    void SkeletalAnimation::SetSkeletonHandle(const AssetHandle &skeletonHandle)
    {
        if (m_SkeletonHandle != AssetHandle(0))
            AssetManager::GetInstance()->RemoveAssetPin(m_SkeletonHandle,
                std::format("skeletal-animtion.{}.{}", static_cast<uint64_t>(handle), static_cast<uint64_t>(m_SkeletonHandle)));

        m_SkeletonHandle = skeletonHandle;
		if (m_SkeletonHandle != AssetHandle(0) && handle != AssetHandle(0))
			AssetManager::GetInstance()->AddAssetPin(m_SkeletonHandle,
				std::format("skeletal-animtion.{}.{}", static_cast<uint64_t>(handle), static_cast<uint64_t>(m_SkeletonHandle)));
    }
}
