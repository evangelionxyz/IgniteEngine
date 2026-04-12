// Copyright (c) 2026 Evangelion Manuhutu

#include "skeleton.hpp"
#include "ignite/serializer/binary_serializer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ignite
{

    glm::mat4 JointSocket::GetLocalTransform() const
    {
        return glm::translate(glm::mat4(1.0f), localTranslation)
            * glm::toMat4(localRotation)
            * glm::scale(glm::mat4(1.0f), localScale);
    }

    std::vector<glm::mat4> Skeleton::GetFinalJointTransforms()
    {
        std::vector<glm::mat4> result;
        result.resize(joints.size());

        for (size_t i = 0; i < joints.size(); ++i)
        {
            const Joint &joint = joints[i];
            result[i] = joint.globalTransform * joint.inverseBindPose;
        }
        return result;
    }

    void Skeleton::UpdateGlobalTransforms()
    {
        // Important optimization: Calculate global transforms in hierarchy order
        for (size_t i = 0; i < joints.size(); ++i)
        {
            Joint &joint = joints[i];

            if (joint.parentJointId == -1)
            {
                // Root joint
                joint.globalTransform = joint.localTransform;
            }
            else
            {
                // Child joint
                joint.globalTransform = joints[joint.parentJointId].globalTransform * joint.localTransform;
            }
        }
    }

    void Skeleton::RebuildSocketMap()
    {
        socketNameToIndex.clear();
        for (size_t i = 0; i < sockets.size(); ++i)
        {
            socketNameToIndex[sockets[i].name] = static_cast<int32_t>(i);
        }
    }

    glm::mat4 Skeleton::GetSocketWorldTransform(const std::string &socketName) const
    {
        const auto it = socketNameToIndex.find(socketName);
        if (it == socketNameToIndex.end())
        {
            return glm::mat4(1.0f);
        }

        const int32_t socketIndex = it->second;
        if (socketIndex < 0 || socketIndex >= static_cast<int32_t>(sockets.size()))
        {
            return glm::mat4(1.0f);
        }

        const JointSocket &socket = sockets[static_cast<size_t>(socketIndex)];
        const glm::mat4 socketLocal = socket.GetLocalTransform();
        if (socket.parentJointId < 0 || socket.parentJointId >= static_cast<int32_t>(joints.size()))
        {
            return socketLocal;
        }

        return joints[static_cast<size_t>(socket.parentJointId)].globalTransform * socketLocal;
    }

    bool Skeleton::Serialize(const std::filesystem::path &filepath)
    {
        BinarySerializer::SerializeSkeleton(this, filepath);
        return true;
    }

    Ref<Skeleton> Skeleton::Deserialize(const std::filesystem::path &filepath)
    {
        auto skeleton = BinarySerializer::DeserializeSkeleton(filepath);
        if (skeleton)
        {
            skeleton->RebuildSocketMap();
        }
        return skeleton;
    }
}
