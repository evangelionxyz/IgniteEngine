// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_DYNAMIC_ACTOR_HPP
#define IGN_PHYSICS_DYNAMIC_ACTOR_HPP

#include "ignite/physics/physics_types.hpp"

namespace ignite::physics
{
	class IGN_PHYSICS_API PhysicsDynamicActor
	{
	public:
		virtual ~PhysicsDynamicActor() = default;

		virtual void AddForce(const glm::vec3 &force) = 0;
		virtual void AddForceAtPosition(const glm::vec3 &force, const glm::vec3 &position) = 0;
		virtual void AddTorque(const glm::vec3 &torque) = 0;
		virtual void AddForceAndTorque(const glm::vec3 &force, const glm::vec3 &torque) = 0;
		virtual void AddAngularImpulse(const glm::vec3 &impulse) = 0;
		virtual void AddImpulse(const glm::vec3 &impulse) = 0;
		virtual void AddImpulseAtPosition(const glm::vec3 &impulse, const glm::vec3 &position) = 0;
		virtual void AddLinearVelocity(const glm::vec3 &velocity) = 0;

		virtual void SetGravityFactor(float value) = 0;
		virtual void SetMaxLinearVelocity(float max) = 0;
		virtual void SetMaxAngularVelocity(float max) = 0;
		virtual void SetLinearDamping(float damping) = 0;
		virtual void SetAngularDamping(float damping) = 0;
		virtual void SetFriction(float value) = 0;
		virtual void SetRestitution(float value) = 0;
		virtual void SetMotionType(BodyType bodyType) = 0;

		virtual void ActivateBody() = 0;
		virtual void DeactivateBody() = 0;
		virtual void DestroyBody() = 0;
		virtual bool IsActive() = 0;

		virtual void SetPosition(const glm::vec3 &position, bool activate) = 0;
		virtual void SetEulerAngleRotation(const glm::vec3 &rotation, bool activate) = 0;
		virtual void SetRotation(const glm::quat &rotation, bool activate) = 0;
		virtual void SetLinearVelocity(const glm::vec3 &vel) = 0;
		virtual void SetAngularVelocity(const glm::vec3 &vel) = 0;

		virtual float GetRestitution() = 0;
		virtual float GetFriction() = 0;
		virtual float GetGravityFactor() = 0;

		virtual glm::vec3 GetPosition() = 0;
		virtual glm::vec3 GetEulerAngles() = 0;
		virtual glm::quat GetRotation() = 0;
		virtual glm::vec3 GetCenterOfMassPosition() = 0;
		virtual glm::vec3 GetLinearVelocity() = 0;
		virtual glm::vec3 GetAngularVelocity() = 0;

		virtual uint64_t GetUserData() = 0;

		static BodyType GetBodyType() { return BodyType::Dynamic; }
	};
}

#endif
