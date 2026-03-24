// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#include "ignite/core/uuid.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "ignite/graphics/vertex_data.hpp"

namespace ignite
{
    struct BoneInfo
    {
        float weights[MAX_BONES] = { 0.0f };
        glm::mat4 offsetMatrix = glm::mat4(1.0f);
    };

    struct BoneMapping
    {
        std::vector<BoneInfo> boneInfo; // Bone weights and indices
        std::unordered_map<std::string, uint32_t> boneMapping; // Maps bone name to indices
    };

    // mesh index to bone info
    using MeshBoneMapping = std::unordered_map<int, BoneMapping>;

    struct Joint
    {
        std::string name;
        int32_t id; // index in joints array
        int32_t parentJointId; // parent in skeleton hierarchy (-1 for root)
        glm::mat4 inverseBindPose; // inverse bind pose matrix
        glm::mat4 localTransform; // current local transform
        glm::vec3 defaultTranslation;
        glm::quat defaultRotation;
        glm::vec3 defaultScale;
        glm::mat4 globalTransform; // current global transform
    };

    class Skeleton : public Asset
    {
    public:
        std::vector<Joint> joints;
        std::unordered_map<std::string, int32_t> nameToJointMap; // for fast lookup by name
        std::unordered_map<int32_t, UUID> jointEntityMap;
        MeshBoneMapping boneMapping;

        static AssetType GetStaticType() { return AssetType::Skeleton; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };

}
