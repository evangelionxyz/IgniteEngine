// Copyright (c) 2026 Evangelion Manuhutu

#include "skeleton.hpp"
#include "ignite/serializer/binary_serializer.hpp"

namespace ignite
{

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

    bool Skeleton::Serialize(const std::filesystem::path &filepath)
	{
		BinarySerializer::SerializeSkeleton(this, filepath);
		return true;
	}

	Ref<Skeleton> Skeleton::Deserialize(const std::filesystem::path &filepath)
	{
		return BinarySerializer::DeserializeSkeleton(filepath);
	}
}
