// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_STATIC_PHYSICS_BODY_HPP
#define IGN_JOLT_STATIC_PHYSICS_BODY_HPP

#include "ignite/physics/3d/physics_static_actor.hpp"

namespace JPH
{
	class BodyInterface;
}

namespace ignite::physics
{
	class IGN_API JoltStaticPhysicsBody : public PhysicsStaticActor
	{
	public:
		JoltStaticPhysicsBody(uint32_t bodyId, JPH::BodyInterface *bodyInterface);
		virtual ~JoltStaticPhysicsBody() override = default;

		virtual void ActivateBody() override;
		virtual void DeactivateBody() override;
		virtual void DestroyBody() override;
		virtual bool IsActive() override;

		virtual void SetPosition(const glm::vec3 &position, bool activate) override;
		virtual void SetEulerAngleRotation(const glm::vec3 &rotation, bool activate) override;
		virtual void SetRotation(const glm::quat &rotation, bool activate) override;
		virtual void SetFriction(float value) override;
		virtual void SetRestitution(float value) override;

		virtual float GetRestitution() override;
		virtual float GetFriction() override;

		virtual glm::vec3 GetPosition() override;
		virtual glm::vec3 GetEulerAngles() override;
		virtual glm::quat GetRotation() override;

		virtual uint64_t GetUserData() override;

		uint32_t GetJoltBodyID() const { return m_BodyId; }

	private:
		uint32_t m_BodyId;
		JPH::BodyInterface *m_BodyInterface;
	};
}

#endif
