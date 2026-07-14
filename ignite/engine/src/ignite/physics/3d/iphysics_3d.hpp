// Copyright (C) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_IPHYSICS_3D_HPP
#define IGN_IPHYSICS_3D_HPP

#include "ignite/core/base.hpp"

namespace ignite
{
	enum class Physics3DType
	{
		Jolt = 0,
		Box3D = 1,
		PhysX = 2,
	};

	class IGN_API IPhysics3D
	{
	public:
		virtual ~IPhysics3D() = default;

		virtual void Init() = 0;
		virtual void Shutdown() = 0;
	};
}

#endif