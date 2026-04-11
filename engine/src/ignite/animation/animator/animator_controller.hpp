// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATOR_CONTROLLER_HPP
#define ANIMATOR_CONTROLLER_HPP

#include "ignite/asset/asset.hpp"

#include "animator.hpp"

namespace ignite
{
    struct AnimState
    {
        std::string name;
        AssetHandle animHandle = AssetHandle(0);; // Skeletal Animation
    };

    class AnimatorController : public Animator, public Asset
    {
    public:
        std::string defaultState;
        std::vector<AnimState> states;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<AnimatorController> Deserialize(const std::filesystem::path &filepath);
        static Ref<AnimatorController> Create();

        static AssetType GetStaticAssetType() { return AssetType::AnimatorController; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
    };
}

#endif