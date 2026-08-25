// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_DYNAMIC_PHYSICS_BODY_HPP
#define IGN_JOLT_DYNAMIC_PHYSICS_BODY_HPP

#include "ignite/physics/3d/physics_dynamic_actor.hpp"

namespace JPH
{
	class BodyInterface;
}

namespace ignite::physics
{
	class IGN_API JoltDynamicPhysicsBody : public PhysicsDynamicActor
	{
	public:
		JoltDynamicPhysicsBody(uint32_t bodyId, JPH::BodyInterface *bodyInterface);
		virtual ~JoltDynamicPhysicsBody() override = default;

		virtual void AddForce(const glm::vec3 &force) override;
		virtual void AddForceAtPosition(const glm::vec3 &force, const glm::vec3 &position) override;
		virtual void AddTorque(const glm::vec3 &torque) override;
		virtual void AddForceAndTorque(const glm::vec3 &force, const glm::vec3 &torque) override;
		virtual void AddAngularImpulse(const glm::vec3 &impulse) override;
		virtual void AddImpulse(const glm::vec3 &impulse) override;
		virtual void AddImpulseAtPosition(const glm::vec3 &impulse, const glm::vec3 &position) override;
		virtual void AddLinearVelocity(const glm::vec3 &velocity) override;

		virtual void SetGravityFactor(float value) override;
		virtual void SetMaxLinearVelocity(float max) override;
		virtual void SetMaxAngularVelocity(float max) override;
		virtual void SetLinearDamping(float damping) override;
		virtual void SetAngularDamping(float damping) override;
		virtual void SetFriction(float value) override;
		virtual void SetRestitution(float value) override;
		virtual void SetMotionType(BodyType bodyType) override;

		virtual void ActivateBody() override;
		virtual void DeactivateBody() override;
		virtual void DestroyBody() override;
		virtual bool IsActive() override;

		virtual void SetPosition(const glm::vec3 &position, bool activate) override;
		virtual void SetEulerAngleRotation(const glm::vec3 &rotation, bool activate) override;
		virtual void SetRotation(const glm::quat &rotation, bool activate) override;
		virtual void SetLinearVelocity(const glm::vec3 &vel) override;
		virtual void SetAngularVelocity(const glm::vec3 &vel) override;

		virtual float GetRestitution() override;
		virtual float GetFriction() override;
		virtual float GetGravityFactor() override;

		virtual glm::vec3 GetPosition() override;
		virtual glm::vec3 GetEulerAngles() override;
		virtual glm::quat GetRotation() override;
		virtual glm::vec3 GetCenterOfMassPosition() override;
		virtual glm::vec3 GetLinearVelocity() override;
		virtual glm::vec3 GetAngularVelocity() override;

		virtual uint64_t GetUserData() override;

		uint32_t GetJoltBodyID() const { return m_BodyId; }

	private:
		uint32_t m_BodyId;
		JPH::BodyInterface *m_BodyInterface;
	};
}

#endif