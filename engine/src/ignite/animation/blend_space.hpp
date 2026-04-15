// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef BLEND_SPACE_HPP
#define BLEND_SPACE_HPP

#include "ignite/asset/asset.hpp"

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace ignite
{
    struct BlendSpaceSample
    {
        AssetHandle animationHandle = AssetHandle(0);
        glm::vec2 position = glm::vec2(0.0f);
    };

    class BlendSpace : public Asset
    {
    public:
        std::string name;
        AssetHandle skeletonHandle = AssetHandle(0);
        std::string axisXName = "Speed";
        std::string axisYName = "Direction";
        glm::vec2 axisMin = glm::vec2(0.0f);
        glm::vec2 axisMax = glm::vec2(1.0f);
        std::vector<BlendSpaceSample> samples;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<BlendSpace> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::BlendSpace; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };
}

#endif