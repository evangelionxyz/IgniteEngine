// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_CHARACTER_CONTROLLER_HPP
#define IGN_JOLT_CHARACTER_CONTROLLER_HPP

#include "ignite/physics/3d/physics_character_controller.hpp"

namespace JPH
{
	class CharacterVirtual;
	class PhysicsSystem;
	class TempAllocator;
}

namespace ignite::physics
{
	class IGN_PHYSICS_API JoltCharacterController : public PhysicsCharacterController
	{
	public:
		JoltCharacterController(const CharacterControllerDesc &desc, uint64_t userData, JPH::PhysicsSystem *physicsSystem, JPH::TempAllocator *tempAllocator);
		virtual ~JoltCharacterController() override;

		virtual void Move(const glm::vec3 &displacement, float deltaTime) override;
		virtual void SetPosition(const glm::vec3 &position) override;
		virtual glm::vec3 GetPosition() const override;

		virtual void SetRotationEuler(const glm::vec3 &eulerRot) override;

		virtual void SetRotation(const glm::quat &rotation) override;
		virtual glm::quat GetRotation() const override;

		virtual void SetLinearVelocity(const glm::vec3 &velocity) override;
		virtual glm::vec3 GetLinearVelocity() const override;

		virtual bool IsOnGround() const override;
		virtual glm::vec3 GetGroundNormal() const override;

		virtual uint64_t GetUserData() const override { return m_UserData; }

		JPH::CharacterVirtual *GetJoltCharacter() const { return m_Character; }

	private:
		uint64_t m_UserData;
		JPH::CharacterVirtual *m_Character = nullptr;
		JPH::PhysicsSystem *m_PhysicsSystem = nullptr;
		JPH::TempAllocator *m_TempAllocator = nullptr;
	};
}

#endif
