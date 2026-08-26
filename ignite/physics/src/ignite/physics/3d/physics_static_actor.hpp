// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_STATIC_ACTOR_HPP
#define IGN_PHYSICS_STATIC_ACTOR_HPP

#include "ignite/physics/physics_types.hpp"

namespace ignite::physics
{
	class IGN_PHYSICS_API PhysicsStaticActor
	{
	public:
		virtual ~PhysicsStaticActor() = default;

		virtual void ActivateBody() = 0;
		virtual void DeactivateBody() = 0;
		virtual void DestroyBody() = 0;
		virtual bool IsActive() = 0;

		virtual void SetPosition(const glm::vec3 &position, bool activate) = 0;
		virtual void SetEulerAngleRotation(const glm::vec3 &rotation, bool activate) = 0;
		virtual void SetRotation(const glm::quat &rotation, bool activate) = 0;
		virtual void SetFriction(float value) = 0;
		virtual void SetRestitution(float value) = 0;

		virtual float GetRestitution() = 0;
		virtual float GetFriction() = 0;

		virtual glm::vec3 GetPosition() = 0;
		virtual glm::vec3 GetEulerAngles() = 0;
		virtual glm::quat GetRotation() = 0;

		virtual uint64_t GetUserData() = 0;

		static BodyType GetBodyType() { return BodyType::Static; }
	};
}

#endif
