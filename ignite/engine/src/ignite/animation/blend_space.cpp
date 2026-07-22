// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "blend_space.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <algorithm>
#include <cmath>

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

namespace ignite
{
	// ------------------------------------
	// Blend Space Sample struct
	BlendSpaceSample::~BlendSpaceSample()
	{
		if (m_AnimationHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_AnimationHandle, std::format("blendspace.sample.{}.{}",
				static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AnimationHandle)));
	}

	void BlendSpaceSample::SetAnimationHandle(const AssetHandle &animationHandle)
	{
		if (m_AnimationHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_AnimationHandle, std::format("blendspace.sample.{}.{}",
				static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AnimationHandle)));

        m_AnimationHandle = animationHandle;
		if (m_AnimationHandle != AssetHandle(0))
			AssetManager::GetInstance()->AddAssetPin(m_AnimationHandle, std::format("blendspace.sample.{}.{}",
				static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AnimationHandle)));
	}

	// ------------------------------------
	// Blend Space Weight struct
	BlendSpaceWeight::~BlendSpaceWeight()
	{
		if (m_AnimationHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_AnimationHandle, std::format("blendspace.weight.{}.{}",
				static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AnimationHandle)));
	}

	void BlendSpaceWeight::SetAnimationHandle(const AssetHandle &animationHandle)
	{
		if (m_AnimationHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_AnimationHandle, std::format("blendspace.weight.{}.{}",
				static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AnimationHandle)));

        m_AnimationHandle = animationHandle;
		if (m_AnimationHandle != AssetHandle(0))
			AssetManager::GetInstance()->AddAssetPin(m_AnimationHandle, std::format("blendspace.weight.{}.{}",
				static_cast<uint64_t>(m_UUID), static_cast<uint64_t>(m_AnimationHandle)));
	}

	// ------------------------------------
    // Blend Space class
	BlendSpace::~BlendSpace()
	{
		if (m_SkeletonHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_SkeletonHandle, std::format("blendspace.{}.{}",
                static_cast<uint64_t>(handle), static_cast<uint64_t>(m_SkeletonHandle)));
	}

	glm::vec2 BlendSpace::ClampInput(const glm::vec2 &input) const
    {
        return glm::clamp(input, glm::min(axisMin, axisMax), glm::max(axisMin, axisMax));
    }

	void BlendSpace::SetSkeletonAssetHandle(AssetHandle skeletonHandle)
	{
		if (m_SkeletonHandle != AssetHandle(0))
			AssetManager::GetInstance()->RemoveAssetPin(m_SkeletonHandle, std::format("blendspace.{}.{}",
				static_cast<uint64_t>(handle), static_cast<uint64_t>(m_SkeletonHandle)));

        m_SkeletonHandle = skeletonHandle;
        if (m_SkeletonHandle != AssetHandle(0) && handle != AssetHandle(0))
            AssetManager::GetInstance()->AddAssetPin(m_SkeletonHandle, std::format("blendspace.{}.{}",
                static_cast<uint64_t>(handle), static_cast<uint64_t>(m_SkeletonHandle)));
	}

	glm::vec2 BlendSpace::NormalizePosition(const glm::vec2 &pos) const
	{
		const float rangeX = axisMax.x - axisMin.x;
		const float rangeY = axisMax.y - axisMin.y;

		const float normX = (rangeX > 0.0001f) ? (pos.x - axisMin.x) / rangeX : 0.0f;
		const float normY = (rangeY > 0.0001f) ? (pos.y - axisMin.y) / rangeY : 0.0f;

		return glm::vec2(normX, normY);
	}

	std::vector<BlendSpaceWeight> BlendSpace::Evaluate(const glm::vec2 &input) const
    {
        struct Candidate
        {
            const BlendSpaceSample *sample;
            float distanceSq;
        };

        std::vector<Candidate> candidates;
        
        const glm::vec2 clamped = ClampInput(input);
        const glm::vec2 normInput = NormalizePosition(clamped);

        for (const BlendSpaceSample &sample : samples)
        {
            if (sample.GetAnimationAssetHandle() == AssetHandle(0))
                continue;

            const glm::vec2 normSamplePos = NormalizePosition(sample.position);
            const float distanceSq = glm::dot(normSamplePos - normInput, normSamplePos - normInput);
            if (distanceSq <= 0.000001f)
            {
                return { BlendSpaceWeight{sample.GetAnimationAssetHandle(), 1.0f} };
            }
            candidates.push_back({ &sample, distanceSq });
        }

        if (candidates.empty())
            return {};

        float total = 0.0f;
        std::vector<BlendSpaceWeight> result;
        result.reserve(candidates.size());
        for (const Candidate &candidate : candidates)
        {
            // Smooth Shepard inverse-distance-squared weighting (exponent 2 on distanceSq).
            // This provides smooth continuous weight transitions without hard-cutoff jumps when crossing grid boundaries.
            const float distSq = std::max(candidate.distanceSq, 0.000001f);
            const float weight = 1.0f / (distSq * distSq);
            result.push_back(BlendSpaceWeight{ candidate.sample->GetAnimationAssetHandle(), weight });
            total += weight;
        }

        if (total > 0.0f)
        {
            for (BlendSpaceWeight &weight : result)
                weight.weight /= total;
        }

        // Prune negligible weights (< 0.001) and renormalize to maintain performance
        std::erase_if(result, [](const BlendSpaceWeight &w) { return w.weight < 0.001f; });
        float remainingTotal = 0.0f;
        for (const auto &w : result)
            remainingTotal += w.weight;

        if (remainingTotal > 0.0f)
        {
            for (auto &w : result)
                w.weight /= remainingTotal;
        }

        return result;
    }

    bool BlendSpace::Serialize(const ignite::Path &filepath)
    {
        Serializer sr(filepath);

        sr.BeginMap();

        sr.BeginMap("BlendSpace");
        sr.AddKeyValue("SkeletonHandle", static_cast<uint64_t>(m_SkeletonHandle));
        sr.AddKeyValue("AxisXName", axisXName);
        sr.AddKeyValue("AxisYName", axisYName);
        sr.AddKeyValue("AxisMin", axisMin);
        sr.AddKeyValue("AxisMax", axisMax);

        sr.BeginSequence("Samples");
        for (auto &sample : samples)
        {
            sr.BeginMap();
            sr.AddKeyValue("AnimationHandle", static_cast<uint64_t>(sample.GetAnimationAssetHandle()));
            sr.AddKeyValue("Position", sample.position);
            sr.EndMap();
        }
        sr.EndSequence();

        sr.EndMap();

        sr.EndMap();
        sr.Serialize();

        SetDirtyFlag(false);
        return true;
    }

    Ref<BlendSpace> BlendSpace::Deserialize(const ignite::Path &filepath)
    {
        if (!ignite::Path::exists(filepath))
            return nullptr;

        YAML::Node root = YAML::LoadFile(filepath.string());
        YAML::Node node = root["BlendSpace"];
        if (!node)
            return nullptr;

        Ref<BlendSpace> blendSpace = BlendSpace::Create();
        if (auto n = node["SkeletonHandle"]) blendSpace->SetSkeletonAssetHandle(AssetHandle(n.as<uint64_t>()));
        if (auto n = node["AxisXName"]) blendSpace->axisXName = n.as<std::string>();
        if (auto n = node["AxisYName"]) blendSpace->axisYName = n.as<std::string>();
        if (auto n = node["AxisMin"]) blendSpace->axisMin = n.as<glm::vec2>();
        if (auto n = node["AxisMax"]) blendSpace->axisMax = n.as<glm::vec2>();

        if (YAML::Node samplesNode = node["Samples"])
        {
			// Reserve space for samples to avoid multiple allocations prevent destroying the sample's animation handle pinning
            blendSpace->samples.reserve(samplesNode.size());
            for (const auto &sampleNode : samplesNode)
            {
                BlendSpaceSample &sample = blendSpace->samples.emplace_back();
                if (auto n = sampleNode["AnimationHandle"]) sample.SetAnimationHandle(AssetHandle(n.as<uint64_t>()));
                if (auto n = sampleNode["Position"]) sample.position = n.as<glm::vec2>();
            }
        }

        blendSpace->SetDirtyFlag(false);
        return blendSpace;
    }

	Ref<BlendSpace> BlendSpace::Create()
	{
		return CreateRef<BlendSpace>();
	}

}
