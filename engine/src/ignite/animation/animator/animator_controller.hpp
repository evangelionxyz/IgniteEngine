// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATOR_CONTROLLER_HPP
#define ANIMATOR_CONTROLLER_HPP

#include "ignite/asset/asset.hpp"

#include "animator.hpp"

#include <glm/glm.hpp>

namespace ignite
{
    // Forward declaration
    class AssetManager;

    struct AnimState
    {
        std::string name;
        AssetHandle animHandle = AssetHandle(0); // Skeletal Animation
        glm::vec2 editorPos = glm::vec2(100.0f, 100.0f);
    };

    struct AnimatorControllerRuntime
    {
        std::string currentStateName;
        float stateElapsed = 0.0f;
        float stateNormalized = 0.0f;
    };

    class AnimatorController : public Animator, public Asset
    {
    public:
        std::string defaultState;
        std::vector<AnimState> states;

        AssetHandle skeletonHandle = AssetHandle(0);

        // Returns new state name if a transition fires, else empty string.
        std::string EvaluateTransitions(const std::string &currentState, float normalizedTime) const;

        // Convenience accessors
        AnimState *FindState(const std::string &name);
        const AnimState *FindState(const std::string &name) const;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<AnimatorController> Deserialize(const std::filesystem::path &filepath);
        static Ref<AnimatorController> Create();

        static AssetType GetStaticAssetType() { return AssetType::AnimatorController; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }

        bool UpdateSkeleton(float deltaTime, AnimatorControllerRuntime &runtime, AssetManager *assetManager);
    };
}

#endif
