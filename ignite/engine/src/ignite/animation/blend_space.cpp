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
    // Blend Space class
    BlendSpace::~BlendSpace() = default;

    glm::vec2 BlendSpace::ClampInput(const glm::vec2 &input) const
    {
        return glm::clamp(input, glm::min(axisMin, axisMax), glm::max(axisMin, axisMax));
    }

    void BlendSpace::SetSkeletonAssetHandle(AssetHandle skeletonHandle)
    {
        m_SkeletonHandle = skeletonHandle;
    }

    glm::vec2 BlendSpace::NormalizePosition(const glm::vec2 &pos) const
    {
        const float rangeX = axisMax.x - axisMin.x;
        const float rangeY = axisMax.y - axisMin.y;

        const float normX = (rangeX > 0.0001f) ? (pos.x - axisMin.x) / rangeX : 0.0f;
        const float normY = (rangeY > 0.0001f) ? (pos.y - axisMin.y) / rangeY : 0.0f;

        return glm::vec2(normX, normY);
    }

    glm::vec2 BlendSpace::SnapToGridPos(const glm::vec2 &pos) const
    {
        const glm::vec2 minVal = glm::min(axisMin, axisMax);
        const glm::vec2 maxVal = glm::max(axisMin, axisMax);
        const glm::vec2 range  = maxVal - minVal;

        const glm::ivec2 divs = glm::max(gridDivisions, glm::ivec2(1));

        const float cellX = (divs.x > 0) ? range.x / static_cast<float>(divs.x) : range.x;
        const float cellY = (divs.y > 0) ? range.y / static_cast<float>(divs.y) : range.y;

        float snappedX = minVal.x + std::round((pos.x - minVal.x) / cellX) * cellX;
        float snappedY = minVal.y + std::round((pos.y - minVal.y) / cellY) * cellY;

        return glm::clamp(glm::vec2(snappedX, snappedY), minVal, maxVal);
    }

    // ------------------------------------
    // Per-axis smoothing advancement
    // ------------------------------------

    static float SmoothStep(float t)
    {
        // Cubic: 3t^2 - 2t^3
        t = glm::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static float SmootherStep(float t)
    {
        // Quintic: 6t^5 - 15t^4 + 10t^3
        t = glm::clamp(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    float BlendSpace::AdvanceAxis(float current, float target, float &vel, float smoothTime,
        BlendSpaceSmoothingType type, float dampRatio, float dt, float axisRange)
    {
        // If smoothing is disabled or time is negligible, snap immediately.
        if (smoothTime < 0.0001f || dt <= 0.0f)
        {
            vel = 0.0f;
            return target;
        }

        const float diff = target - current;
        if (std::abs(diff) < 0.0001f && (type != BlendSpaceSmoothingType::SpringDamper || std::abs(vel) < 0.0001f))
        {
            vel = 0.0f;
            return target;
        }

        switch (type)
        {
        case BlendSpaceSmoothingType::Averaged:
        {
            // Exponential moving average: reaches ~95% of target in smoothTime seconds
            const float rate = 3.0f / smoothTime;
            const float alpha = 1.0f - std::exp(-rate * dt);
            float nextVal = current + diff * alpha;
            if (std::abs(target - nextVal) < 0.0001f)
                nextVal = target;
            return nextVal;
        }

        case BlendSpaceSmoothingType::Linear:
        {
            // Constant speed interpolation towards target
            const float speed = (axisRange > 0.0001f) ? (axisRange / smoothTime) : (std::abs(diff) / smoothTime);
            const float step = speed * dt;
            if (std::abs(diff) <= step)
                return target;
            return current + (diff > 0.0f ? step : -step);
        }

        case BlendSpaceSmoothingType::Cubic:
        {
            // Cubic easing transition towards target
            const float rate = 3.0f / smoothTime;
            const float alpha = 1.0f - std::exp(-rate * dt);
            const float cubicAlpha = SmoothStep(alpha);
            float nextVal = current + diff * cubicAlpha;
            if (std::abs(target - nextVal) < 0.0001f)
                nextVal = target;
            return nextVal;
        }

        case BlendSpaceSmoothingType::EaseInOut:
        {
            // Quintic easing transition towards target
            const float rate = 3.5f / smoothTime;
            const float alpha = 1.0f - std::exp(-rate * dt);
            const float easedAlpha = SmootherStep(alpha);
            float nextVal = current + diff * easedAlpha;
            if (std::abs(target - nextVal) < 0.0001f)
                nextVal = target;
            return nextVal;
        }

        case BlendSpaceSmoothingType::Exponential:
        {
            // Fast exponential decay: reaches 99.3% in smoothTime seconds
            const float rate = 5.0f / smoothTime;
            const float alpha = 1.0f - std::exp(-rate * dt);
            float nextVal = current + diff * alpha;
            if (std::abs(target - nextVal) < 0.0001f)
                nextVal = target;
            return nextVal;
        }

        case BlendSpaceSmoothingType::SpringDamper:
        default:
        {
            // Unconditionally stable 2nd-order implicit spring-damper
            const float x = -diff; // displacement from target
            const float omega = 2.0f * glm::pi<float>() / smoothTime;
            const float zeta = std::max(0.001f, dampRatio);

            const float D = 1.0f + 2.0f * zeta * omega * dt + omega * omega * dt * dt;
            vel = (vel - omega * omega * x * dt) / D;
            const float newX = x + vel * dt;
            float nextVal = target + newX;

            if (std::abs(nextVal - target) < 0.0001f && std::abs(vel) < 0.0001f)
            {
                vel = 0.0f;
                return target;
            }

            return nextVal;
        }
        }
    }

    glm::vec2 BlendSpace::AdvanceSmoothedInput(const glm::vec2 &rawInput, glm::vec2 &smoothedInput, glm::vec2 &velocity, float deltaTime) const
    {
        const float rangeX = std::abs(axisMax.x - axisMin.x);
        const float rangeY = std::abs(axisMax.y - axisMin.y);

        smoothedInput.x = AdvanceAxis(smoothedInput.x, rawInput.x,
            velocity.x, smoothingTime.x, smoothingType, dampingRatio, deltaTime, rangeX);

        smoothedInput.y = AdvanceAxis(smoothedInput.y, rawInput.y,
            velocity.y, smoothingTime.y, smoothingType, dampingRatio, deltaTime, rangeY);

        return smoothedInput;
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

    bool BlendSpace::Serialize(const std::filesystem::path &filepath)
    {
        Serializer sr(filepath);

        sr.BeginMap();

        sr.BeginMap("BlendSpace");
        sr.AddKeyValue("SkeletonHandle", static_cast<uint64_t>(m_SkeletonHandle));
        sr.AddKeyValue("AxisXName", axisXName);
        sr.AddKeyValue("AxisYName", axisYName);
        sr.AddKeyValue("AxisMin", axisMin);
        sr.AddKeyValue("AxisMax", axisMax);

        // Grid
        sr.AddKeyValue("GridDivisionsX", gridDivisions.x);
        sr.AddKeyValue("GridDivisionsY", gridDivisions.y);
        sr.AddKeyValue("SnapToGrid", snapToGrid);

        // Smoothing
        sr.AddKeyValue("SmoothingTimeX", smoothingTime.x);
        sr.AddKeyValue("SmoothingTimeY", smoothingTime.y);
        sr.AddKeyValue("SmoothingType", static_cast<int>(smoothingType));
        sr.AddKeyValue("DampingRatio", dampingRatio);

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

    Ref<BlendSpace> BlendSpace::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
            return nullptr;

        YAML::Node root = YAML::LoadFile(filepath.string());
        YAML::Node node = root["BlendSpace"];
        if (!node)
            return nullptr;

        Ref<BlendSpace> blendSpace = BlendSpace::Create();
        if (auto n = node["SkeletonHandle"]) blendSpace->SetSkeletonAssetHandle(AssetHandle(n.as<uint64_t>()));
        if (auto n = node["AxisXName"])      blendSpace->axisXName = n.as<std::string>();
        if (auto n = node["AxisYName"])      blendSpace->axisYName = n.as<std::string>();
        if (auto n = node["AxisMin"])        blendSpace->axisMin   = n.as<glm::vec2>();
        if (auto n = node["AxisMax"])        blendSpace->axisMax   = n.as<glm::vec2>();

        // Grid (backward-compatible: missing keys fall back to defaults)
        if (auto n = node["GridDivisionsX"]) blendSpace->gridDivisions.x = n.as<int>();
        if (auto n = node["GridDivisionsY"]) blendSpace->gridDivisions.y = n.as<int>();
        if (auto n = node["SnapToGrid"])     blendSpace->snapToGrid       = n.as<bool>();

        // Smoothing (backward-compatible)
        if (auto n = node["SmoothingTimeX"]) blendSpace->smoothingTime.x  = n.as<float>();
        if (auto n = node["SmoothingTimeY"]) blendSpace->smoothingTime.y  = n.as<float>();
        if (auto n = node["SmoothingType"])  blendSpace->smoothingType    = static_cast<BlendSpaceSmoothingType>(n.as<int>());
        if (auto n = node["DampingRatio"])   blendSpace->dampingRatio     = n.as<float>();

        if (YAML::Node samplesNode = node["Samples"])
        {
            // Reserve space for samples to avoid multiple allocations prevent destroying the sample's animation handle pinning
            blendSpace->samples.reserve(samplesNode.size());
            for (const auto &sampleNode : samplesNode)
            {
                BlendSpaceSample &sample = blendSpace->samples.emplace_back();
                if (auto n = sampleNode["AnimationHandle"]) sample.SetAnimationHandle(AssetHandle(n.as<uint64_t>()));
                if (auto n = sampleNode["Position"])        sample.position = n.as<glm::vec2>();
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
