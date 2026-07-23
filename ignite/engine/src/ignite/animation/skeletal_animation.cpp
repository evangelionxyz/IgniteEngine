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

    AnimationTimelineEvent::~AnimationTimelineEvent()
    {
        UnpinAssets();
    }

    void AnimationTimelineEvent::UnpinAssets()
    {
        if (m_AudioHandle != AssetHandle(0))
        {
            if (auto *assetManager = AssetManager::GetInstance())
            {
                assetManager->RemoveAssetPin(m_AudioHandle, std::format("animevent.audio.{}.{}", static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AudioHandle)));
            }
        }
        if (m_CallbackAsset != AssetHandle(0))
        {
            if (auto *assetManager = AssetManager::GetInstance())
            {
                assetManager->RemoveAssetPin(m_CallbackAsset, std::format("animevent.callback.{}.{}", static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_CallbackAsset)));
            }
        }
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
            UnpinAssets();
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
        if (m_AudioHandle != handle)
        {
            if (m_AudioHandle != AssetHandle(0))
            {
                if (auto *assetManager = AssetManager::GetInstance())
                {
                    assetManager->RemoveAssetPin(m_AudioHandle, std::format("animevent.audio.{}.{}", static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AudioHandle)));
                }
            }
            m_AudioHandle = handle;
            if (m_AudioHandle != AssetHandle(0))
            {
                if (auto *assetManager = AssetManager::GetInstance())
                {
                    assetManager->AddAssetPin(m_AudioHandle, std::format("animevent.audio.{}.{}", static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AudioHandle)));
                }
            }
        }
    }

    void AnimationTimelineEvent::SetCallbackAsset(const AssetHandle &handle)
    {
        if (m_CallbackAsset != handle)
        {
            if (m_CallbackAsset != AssetHandle(0))
            {
                if (auto *assetManager = AssetManager::GetInstance())
                {
                    assetManager->RemoveAssetPin(m_CallbackAsset, std::format("animevent.callback.{}.{}", static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_CallbackAsset)));
                }
            }
            m_CallbackAsset = handle;
            if (m_CallbackAsset != AssetHandle(0))
            {
                if (auto *assetManager = AssetManager::GetInstance())
                {
                    assetManager->AddAssetPin(m_CallbackAsset, std::format("animevent.callback.{}.{}", static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_CallbackAsset)));
                }
            }
        }
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
