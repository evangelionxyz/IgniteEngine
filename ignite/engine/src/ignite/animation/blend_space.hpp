// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BLEND_SPACE_HPP
#define IGN_BLEND_SPACE_HPP

#include "ignite/asset/asset.hpp"

#include <vector>
#include <glm/glm.hpp>

namespace ignite
{
    struct IGN_API BlendSpaceSample
    {
        glm::vec2 position = glm::vec2(0.0f);

        BlendSpaceSample() = default;

        void SetAnimationHandle(const AssetHandle &animationHandle) { m_AnimationHandle = animationHandle; }
        const AssetHandle &GetAnimationAssetHandle() const { return m_AnimationHandle; }

    private:
        AssetHandle m_AnimationHandle;
    };

    struct IGN_API BlendSpaceWeight
    {
        float weight = 0.0f;

        BlendSpaceWeight() = default;

        BlendSpaceWeight(const AssetHandle &animationHandle, float weight)
            : m_AnimationHandle(animationHandle), weight(weight)
        {
        }

		void SetAnimationHandle(const AssetHandle &animationHandle) { m_AnimationHandle = animationHandle; }
		const AssetHandle &GetAnimationAssetHandle() const { return m_AnimationHandle; }

    private:
		AssetHandle m_AnimationHandle;
    };

    // Determines which easing function is used when smoothing BlendSpace inputs.
    enum class IGN_API BlendSpaceSmoothingType : uint8_t
    {
        Averaged    = 0, // Simple lerp — weighted average toward target
        Linear      = 1, // Constant-speed move toward target (no overshoot)
        Cubic       = 2, // Smoothstep cubic ease
        EaseInOut   = 3, // Smootherstep quintic ease (smoother start/end)
        Exponential = 4, // Exponential decay approach
        SpringDamper = 5 // Spring-damper (default); supports overshoot when dampingRatio < 1
    };

    class IGN_API BlendSpace : public Asset
    {
    public:
        virtual ~BlendSpace() override;

        // ---- Axes ----
        std::string axisXName = "Horizontal";
        std::string axisYName = "Vertical";
        glm::vec2 axisMin = glm::vec2(0.0f);
        glm::vec2 axisMax = glm::vec2(1.0f);

        // ---- Grid ----
        // Number of visual subdivisions on each axis in the editor canvas.
        glm::ivec2 gridDivisions = glm::ivec2(10, 10);
        // When true, dragging sample points snaps to the nearest grid cell.
        bool snapToGrid = false;

        // ---- Smoothing ----
        // Per-axis smoothing time in seconds. 0 = instant (no smoothing).
        glm::vec2 smoothingTime = glm::vec2(0.0f, 0.0f);
        // Which easing function drives the input smoothing.
        BlendSpaceSmoothingType smoothingType = BlendSpaceSmoothingType::SpringDamper;
        // Damping ratio for SpringDamper mode.
        //   = 1.0 : critically damped (no overshoot, fastest settle)
        //   < 1.0 : under-damped (overshoot — can look more natural)
        //   > 1.0 : over-damped (slow, no overshoot)
        float dampingRatio = 1.0f;

        // ---- Samples ----
        std::vector<BlendSpaceSample> samples;

        static Ref<BlendSpace> Create();

        void SetSkeletonAssetHandle(AssetHandle skeletonHandle);
        const AssetHandle &GetSkeletonAssetHandle() const { return m_SkeletonHandle; }

        // Returns normalized weights for the samples nearest to input.  The
        // inverse-distance form deliberately supports sparse and irregular 2D
        // layouts while still giving an exact sample a weight of one.
        std::vector<BlendSpaceWeight> Evaluate(const glm::vec2 &input) const;
        glm::vec2 ClampInput(const glm::vec2 &input) const;
        glm::vec2 NormalizePosition(const glm::vec2 &pos) const;

        // Snap a grid-space position to the nearest grid cell corner.
        glm::vec2 SnapToGridPos(const glm::vec2 &pos) const;

        // Advance per-instance smoothed BlendSpace input toward rawInput.
        //   smoothedInput  — the current smoothed value (mutable runtime state, lives in AnimatorControllerRuntime)
        //   velocity       — spring velocity state, only used by SpringDamper (lives in AnimatorControllerRuntime)
        //   deltaTime      — frame delta in seconds
        // Returns the new smoothed input to pass to Evaluate().
        glm::vec2 AdvanceSmoothedInput(const glm::vec2 &rawInput, glm::vec2 &smoothedInput,
            glm::vec2 &velocity, float deltaTime) const;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<BlendSpace> Deserialize(const ignite::Path &filepath);

        static AssetType GetStaticType() { return AssetType::BlendSpace; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        AssetHandle m_SkeletonHandle = AssetHandle(0);

        // Per-axis smoothing helpers
        static float AdvanceAxis(float current, float target, float &vel, float smoothTime,
            BlendSpaceSmoothingType type, float dampingRatio, float dt, float axisRange = 0.0f);
    };
}

#endif
