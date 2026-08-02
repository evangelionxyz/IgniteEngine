// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ANIMATOR_CONTROLLER_HPP
#define IGN_ANIMATOR_CONTROLLER_HPP

#include "ignite/asset/asset.hpp"

#include "animator.hpp"
#include "ignite/math/transform.hpp"

namespace ignite
{
    // Forward declaration
    class AssetManager;

    struct IGN_API AnimState
    {
        enum class MotionType : uint8_t { SkeletalAnimation, BlendSpace };

        std::string name;
        glm::vec2 editorPos = glm::vec2(100.0f, 100.0f);

        AnimState() = default;
        AnimState(const AnimState &other)
            : name(other.name), editorPos(other.editorPos)
            , m_MotionType(other.m_MotionType), m_MotionHandle(other.m_MotionHandle)
			, m_UUID(other.m_UUID)
        {
        }

        AnimState &operator=(const AnimState &other)
        {
            if (this != &other)
            {
                name = other.name;
                editorPos = other.editorPos;
                m_UUID = UUID();
                m_MotionType = other.m_MotionType;
                m_MotionHandle = other.m_MotionHandle;
            }
            return *this;
        }

		~AnimState();

        void SetAnimationHandle(const AssetHandle &animationHandle);
        void SetBlendSpaceHandle(const AssetHandle &blendSpaceHandle);
        void SetMotion(MotionType type, const AssetHandle &motionHandle);
        MotionType GetMotionType() const { return m_MotionType; }
        const AssetHandle &GetMotionHandle() const { return m_MotionHandle; }
		const AssetHandle &GetAnimationAssetHandle() const { return m_MotionHandle; }

    private:
        UUID m_UUID;
        MotionType m_MotionType = MotionType::SkeletalAnimation;
        AssetHandle m_MotionHandle = AssetHandle(0);
    };

    struct IGN_API AnimatorControllerRuntime
    {
        std::string currentStateName;
        float stateElapsed = 0.0f;
        float stateNormalized = 0.0f;
        float previousStateNormalized = 0.0f;

        // Transition blending state
        bool isTransitioning = false;
        std::string transitionTargetState;
        float transitionDuration = 0.0f;
        float transitionElapsed = 0.0f;

        AssetHandle eventSourceAnimation = AssetHandle(0);
        std::vector<uint32_t> triggeredEventIndices;

        std::vector<Transform> localPoses;
        std::vector<Transform> globalPoses;
        std::vector<glm::mat4> finalTransforms; // per-instance GPU-ready bone transforms

        // Root motion
        glm::vec3 rootMotionDelta = glm::vec3(0.0f);
        bool hasRootMotion = false;

        // BlendSpace per-instance smoothing state.
        // Smoothed input advances toward the raw animator param values each frame
        // using the smoothing settings on the active BlendSpace asset.
        glm::vec2 blendSpaceSmoothedInput = glm::vec2(0.0f);
        // Spring-damper velocity state (only used when smoothingType == SpringDamper).
        glm::vec2 blendSpaceVelocity = glm::vec2(0.0f);
    };

    class IGN_API AnimatorController : public Animator, public Asset
    {
    public:
        std::string defaultState;
        std::unordered_map<std::string, AnimState> states;

        virtual ~AnimatorController() override;

        static Ref<AnimatorController> Clone(const Ref<AnimatorController> &other);

        void SetSkeletonHandle(const AssetHandle &skeletonHandle);
        const AssetHandle &GetSkeletonHandle() const { return m_SkeletonHandle; }

        // Returns new state name if a transition fires, else empty string.
        std::string EvaluateTransitions(const std::string &currentState, float normalizedTime) const;
        const AnimTransition *FindMatchingTransition(const std::string &currentState, float normalizedTime) const;

        // Convenience accessors
        AnimState *FindState(const std::string &name);
        const AnimState *FindState(const std::string &name) const;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<AnimatorController> Deserialize(const ignite::Path &filepath);
        static Ref<AnimatorController> Create();

        static AssetType GetStaticAssetType() { return AssetType::AnimatorController; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }

        bool UpdateSkeleton(float deltaTime, AnimatorControllerRuntime &runtime, AssetManager *assetManager);

    private:
		AssetHandle m_SkeletonHandle = AssetHandle(0);
    };
}

#endif
