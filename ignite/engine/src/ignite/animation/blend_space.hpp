// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BLEND_SPACE_HPP
#define IGN_BLEND_SPACE_HPP

#include "ignite/asset/asset.hpp"

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace ignite
{
    struct IGN_API BlendSpaceSample
    {
        AssetHandle animationHandle = AssetHandle(0);
        glm::vec2 position = glm::vec2(0.0f);
    };

    class IGN_API BlendSpace : public Asset
    {
    public:
        std::string name;
        AssetHandle skeletonHandle = AssetHandle(0);
        std::string axisXName = "Horizontal";
        std::string axisYName = "Vertical";
        glm::vec2 axisMin = glm::vec2(0.0f);
        glm::vec2 axisMax = glm::vec2(1.0f);
        std::vector<BlendSpaceSample> samples;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<BlendSpace> Deserialize(const ignite::Path &filepath);

        static AssetType GetStaticType() { return AssetType::BlendSpace; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };
}

#endif