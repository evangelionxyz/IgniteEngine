// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SKELETON_HPP
#define IGN_SKELETON_HPP

#include "ignite/core/uuid.hpp"
#include "ignite/asset/asset.hpp"

#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/math/transform.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace ignite
{
    struct IGN_API BoneInfo
    {
        float weights[MAX_BONES] = { 0.0f };
        glm::mat4 offsetMatrix = glm::mat4(1.0f);
    };

    struct IGN_API Joint
    {
        std::string name;
        int32_t id; // index in joints array
        int32_t parentJointId; // parent in skeleton hierarchy (-1 for root)

        Transform defaultTransform;

        // on runtime calculation
        glm::mat4 inverseBindPose; // inverse bind pose matrix
        glm::mat4 localTransform; // current local transform
        glm::mat4 globalTransform; // current global transform
    };

    struct IGN_API JointSocket
    {
        std::string name;
        int32_t parentJointId = -1;
        Transform local;
    };

    class IGN_API Skeleton : public Asset
    {
    public:
        std::vector<Joint> joints;
        std::unordered_map<std::string, int32_t> nameToJointMap;

        std::vector<JointSocket> sockets;
        std::unordered_map<std::string, int32_t> socketNameToIndex;

        std::vector<glm::mat4> GetFinalJointTransforms();
        void UpdateGlobalTransforms();
        void RebuildSocketMap();
        glm::mat4 GetSocketWorldTransform(const std::string &socketName) const;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Skeleton> Deserialize(const std::filesystem::path &filepath);

        static AssetType GetStaticType() { return AssetType::Skeleton; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }
    };
}

#endif
