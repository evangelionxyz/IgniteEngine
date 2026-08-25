// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_CHARACTER_CONTROLLER_HPP
#define IGN_PHYSICS_CHARACTER_CONTROLLER_HPP

#include "ignite/physics/physics_types.hpp"

namespace ignite::physics
{
	class IGN_API PhysicsCharacterController
	{
	public:
		virtual ~PhysicsCharacterController() = default;

		virtual void Move(const glm::vec3 &displacement, float deltaTime) = 0;
		virtual void SetPosition(const glm::vec3 &position) = 0;
		virtual glm::vec3 GetPosition() const = 0;

		virtual void SetRotationEuler(const glm::vec3 &eulerRot) = 0;

		virtual void SetRotation(const glm::quat &rotation) = 0;
		virtual glm::quat GetRotation() const = 0;

		virtual void SetLinearVelocity(const glm::vec3 &velocity) = 0;
		virtual glm::vec3 GetLinearVelocity() const = 0;

		virtual bool IsOnGround() const = 0;
		virtual glm::vec3 GetGroundNormal() const = 0;

		virtual uint64_t GetUserData() const = 0;
	};
}

#endif
