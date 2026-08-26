// Copyright (c) 2026 Evangelion Manuhutu

#include "physics_3d.hpp"
#include "jolt/jolt_physics.hpp"

namespace ignite::physics
{
	Scope<Physics3D> Physics3D::Create(Physics3DType type)
	{
		switch (type)
		{
		case Physics3DType::Jolt:
			return CreateScope<JoltPhysics>();
		default:
			return nullptr;
		}
	}
}
