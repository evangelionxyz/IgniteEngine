// Copyright (c) 2026 Evangelion Manuhutu

#include "skeleton.hpp"
#include "ignite/serializer/binary_serializer.hpp"

namespace ignite
{
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

