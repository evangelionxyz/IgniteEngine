// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_JOINT_HPP
#define IGN_JOLT_JOINT_HPP

#include "ignite/physics/3d/physics_joint.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

namespace JPH
{
	class PhysicsSystem;
	class BodyInterface;
}

namespace ignite::physics
{
	class IGN_PHYSICS_API JoltJoint : public PhysicsJoint
	{
	public:
		JoltJoint(
			const JointDesc &desc,
			uint64_t userData,
			JPH::PhysicsSystem *physicsSystem,
			JPH::BodyInterface *bodyInterface,
			JPH::BodyID bodyID1,
			JPH::BodyID bodyID2,
			const TransformDesc &transformA,
			const TransformDesc &transformB
		);

		virtual ~JoltJoint() override;

		virtual void DestroyJoint() override;

		virtual JointType GetJointType() const override { return m_JointType; }

		virtual void SetEnabled(bool enabled) override;
		virtual bool IsEnabled() const override;
		virtual bool IsBroken() const override { return m_IsBroken; }

		virtual void UpdateFromDesc(const JointDesc &desc) override;
		virtual bool CheckBreakCondition(float deltaTime) override;

		virtual uint64_t GetUserData() const override { return m_UserData; }

		JPH::TwoBodyConstraint *GetJoltConstraint() const { return m_Constraint.GetPtr(); }

	private:
		uint64_t m_UserData = 0;
		JointType m_JointType = JointType::Fixed;
		float m_BreakForce = 0.0f;
		float m_BreakTorque = 0.0f;
		bool m_IsBroken = false;
		bool m_AddedToSystem = false;

		JPH::Ref<JPH::TwoBodyConstraint> m_Constraint = nullptr;
		JPH::PhysicsSystem *m_PhysicsSystem = nullptr;
		JPH::BodyInterface *m_BodyInterface = nullptr;
	};
}

#endif
