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

    AnimationTimelineEvent::AnimationTimelineEvent() = default;

    AnimationTimelineEvent::~AnimationTimelineEvent() = default;

    void AnimationTimelineEvent::UnpinAssets()
    {
    }

    AnimationTimelineEvent::AnimationTimelineEvent(const AnimationTimelineEvent &other)
        : normalizedTime(other.normalizedTime)
        , name(other.name)
        , action(other.action)
    {
        SetAudioHandle(other.m_AudioHandle);
        SetCallbackAsset(other.m_CallbackAsset);
    }

    AnimationTimelineEvent &AnimationTimelineEvent::operator=(const AnimationTimelineEvent &other)
    {
        if (this != &other)
        {
            normalizedTime = other.normalizedTime;
            name = other.name;
            action = other.action;
            SetAudioHandle(other.m_AudioHandle);
            SetCallbackAsset(other.m_CallbackAsset);
        }
        return *this;
    }

    AnimationTimelineEvent::AnimationTimelineEvent(AnimationTimelineEvent &&other) noexcept
        : normalizedTime(other.normalizedTime)
        , name(std::move(other.name))
        , action(other.action)
        , m_UUID(other.m_UUID)
        , m_AudioHandle(other.m_AudioHandle)
        , m_CallbackAsset(other.m_CallbackAsset)
    {
        other.m_AudioHandle = AssetHandle(0);
        other.m_CallbackAsset = AssetHandle(0);
    }

    AnimationTimelineEvent &AnimationTimelineEvent::operator=(AnimationTimelineEvent &&other) noexcept
    {
        if (this != &other)
        {
            normalizedTime = other.normalizedTime;
            name = std::move(other.name);
            action = other.action;
            m_UUID = other.m_UUID;
            m_AudioHandle = other.m_AudioHandle;
            m_CallbackAsset = other.m_CallbackAsset;
            other.m_AudioHandle = AssetHandle(0);
            other.m_CallbackAsset = AssetHandle(0);
        }
        return *this;
    }

    void AnimationTimelineEvent::SetAudioHandle(const AssetHandle &handle)
    {
        m_AudioHandle = handle;
    }

    void AnimationTimelineEvent::SetCallbackAsset(const AssetHandle &handle)
    {
        m_CallbackAsset = handle;
    }

	SkeletalAnimation::~SkeletalAnimation() = default;

	bool SkeletalAnimation::Serialize(const std::filesystem::path &filepath)
    {
        BinarySerializer::SerializeSkeletalAnimation(this, filepath);
        SetDirtyFlag(false);
        return true;
    }

    Ref<SkeletalAnimation> SkeletalAnimation::Deserialize(const std::filesystem::path &filepath)
	{
        return BinarySerializer::DeserializeSkeletalAnimation(filepath);
	}

    void SkeletalAnimation::SetSkeletonHandle(const AssetHandle &skeletonHandle)
    {
        m_SkeletonHandle = skeletonHandle;
    }
}
