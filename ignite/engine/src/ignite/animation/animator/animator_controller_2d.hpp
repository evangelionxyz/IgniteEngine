// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ANIMATOR_CONTROLLER_2D_HPP
#define IGN_ANIMATOR_CONTROLLER_2D_HPP

#include "ignite/asset/asset.hpp"

#include "animator.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "ignite/core/path.hpp"

namespace ignite
{
    struct IGN_API AnimState2D
    {
        std::string name;
        AssetHandle animHandle = AssetHandle(0); // Anim 2D
        glm::vec2 editorPos = glm::vec2(100.0f, 100.0f);
    };

    class IGN_API AnimatorController2D : public Animator, public Asset
    {
    public:
        std::string defaultState;
        std::vector<AnimState2D> states;

        // Returns new state name if a transition fires, else empty string.
        std::string EvaluateTransitions(const std::string &currentState, float normalizedTime) const;

        // Convenience accessors
        AnimState2D       *FindState(const std::string &name);
        const AnimState2D *FindState(const std::string &name) const;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<AnimatorController2D> Deserialize(const ignite::Path &filepath);
        static Ref<AnimatorController2D> Create();

        static AssetType GetStaticAssetType() { return AssetType::AnimatorController2D; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
    };
}

#endif
