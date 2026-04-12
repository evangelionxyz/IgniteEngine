// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATOR_CONTROLLER_2D_HPP
#define ANIMATOR_CONTROLLER_2D_HPP

#include "ignite/asset/asset.hpp"

#include "animator.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <filesystem>

namespace ignite
{
    struct AnimState2D
    {
        std::string name;
        AssetHandle animHandle = AssetHandle(0); // Anim 2D
        glm::vec2 editorPos = glm::vec2(100.0f, 100.0f);
    };

    class AnimatorController2D : public Animator, public Asset
    {
    public:
        std::string defaultState;
        std::vector<AnimState2D> states;

        // Returns new state name if a transition fires, else empty string.
        std::string EvaluateTransitions(const std::string &currentState, float normalizedTime) const;

        // Convenience accessors
        AnimState2D       *FindState(const std::string &name);
        const AnimState2D *FindState(const std::string &name) const;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<AnimatorController2D> Deserialize(const std::filesystem::path &filepath);
        static Ref<AnimatorController2D> Create();

        static AssetType GetStaticAssetType() { return AssetType::AnimatorController2D; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
    };
}

#endif
