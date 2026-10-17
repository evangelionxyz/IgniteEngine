// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_JOINT_HPP
#define IGN_PHYSICS_JOINT_HPP

#include "ignite/physics/physics_types.hpp"

namespace ignite::physics
{
	class IGN_PHYSICS_API PhysicsJoint
	{
	public:
		virtual ~PhysicsJoint() = default;

		virtual void DestroyJoint() = 0;

		virtual JointType GetJointType() const = 0;

		virtual void SetEnabled(bool enabled) = 0;
		virtual bool IsEnabled() const = 0;
		virtual bool IsBroken() const = 0;

		virtual void UpdateFromDesc(const JointDesc &desc) = 0;
		virtual bool CheckBreakCondition(float deltaTime) = 0;

		virtual uint64_t GetUserData() const = 0;
	};
}

#endif
