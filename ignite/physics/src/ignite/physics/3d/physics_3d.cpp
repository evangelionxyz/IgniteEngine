// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "physics_3d.hpp"
#include "jolt/jolt_physics.hpp"
#include "ignite/physics/physics_log.hpp"

namespace ignite::physics
{
	Scope<Physics3D> Physics3D::Create(Physics3DType type)
	{
		switch (type)
		{
		case Physics3DType::Jolt:
			return CreateScope<JoltPhysics>();
		default:
			IGN_PHYSICS_ERROR("[Physics3D] Unknown 3D Physics engine type: {}", static_cast<int>(type));
			return nullptr;
		}
	}
}
