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
        std::string name;
        glm::vec2 editorPos = glm::vec2(100.0f, 100.0f);

        void SetAnimationHandle(const AssetHandle &animationHandle);
		const AssetHandle &GetAnimationAssetHandle() const { return m_AnimHandle; }

        ~AnimState();

    private:
        UUID m_UUID;
        AssetHandle m_AnimHandle = AssetHandle(0); // Skeletal Animation
    };

    struct IGN_API AnimatorControllerRuntime
    {
        std::string currentStateName;
        float stateElapsed = 0.0f;
        float stateNormalized = 0.0f;

        std::vector<Transform> localPoses;
        std::vector<Transform> globalPoses;
        std::vector<glm::mat4> finalTransforms; // per-instance GPU-ready bone transforms
    };

    class IGN_API AnimatorController : public Animator, public Asset
    {
    public:
        std::string defaultState;
        std::vector<AnimState> states;

        virtual ~AnimatorController() override;

        void SetSkeletonHandle(const AssetHandle &skeletonHandle);
        const AssetHandle &GetSkeletonHandle() const { return m_SkeletonHandle; }

        // Returns new state name if a transition fires, else empty string.
        std::string EvaluateTransitions(const std::string &currentState, float normalizedTime) const;

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
