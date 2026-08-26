// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_LOCOMOTION_HPP
#define IGN_LOCOMOTION_HPP

#include "ignite/asset/asset.hpp"

#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace ignite
{
    struct IGN_API LocomotionState
    {
        std::string name;
        bool useBlendSpace = false;
        AssetHandle assetHandle = AssetHandle(0); // .ixanim or .bsp
    };

    class IGN_API LocomotionController : public Asset
    {
    public:
        std::string name;
        AssetHandle skeletonHandle = AssetHandle(0);
        std::string defaultState;
        std::vector<LocomotionState> states;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<LocomotionController> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::LocomotionController; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };
}

#endif
