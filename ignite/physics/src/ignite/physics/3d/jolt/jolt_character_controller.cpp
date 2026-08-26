// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_character_controller.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "ignite/core/logger.hpp"

namespace ignite::physics
{
	static JPH::Vec3 GlmToJoltVec3(const glm::vec3 &v)
	{
		return { v.x, v.y, v.z };
	}

	static glm::vec3 JoltToGlmVec3(const JPH::Vec3 &v)
	{
		return { v.GetX(), v.GetY(), v.GetZ() };
	}

	static JPH::Quat GlmToJoltQuat(const glm::quat &q)
	{
		return { q.x, q.y, q.z, q.w };
	}

	static inline glm::quat JoltToGlmQuat(const JPH::Quat &q)
	{
		return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
	}

	JoltCharacterController::JoltCharacterController(const CharacterControllerDesc &desc, uint64_t userData, JPH::PhysicsSystem *physicsSystem, JPH::TempAllocator *tempAllocator)
		: m_UserData(userData)
		, m_PhysicsSystem(physicsSystem)
		, m_TempAllocator(tempAllocator)
	{
        LOG_ASSERT(m_PhysicsSystem, "[Jolt Physics] Physics system is not valid!");

		JPH::CapsuleShapeSettings capsuleSettings(desc.halfHeight, desc.radius);
		JPH::ShapeRefC innerShape = capsuleSettings.Create().Get();
		JPH::RefConst<JPH::Shape> standingShape = JPH::RotatedTranslatedShapeSettings(
			JPH::Vec3(desc.center.x, desc.center.y, desc.center.z),
			JPH::Quat::sIdentity(),
			innerShape
		).Create().Get();

		JPH::CharacterVirtualSettings settings;
		settings.mShape = standingShape;
		settings.mMass = desc.mass;
		settings.mMaxSlopeAngle = glm::radians(desc.maxSlopeAngle);
		settings.mMaxStrength = 100.0f;
		settings.mPredictiveContactDistance = 0.1f;
		settings.mUp = GlmToJoltVec3(desc.up);

		m_Character = new JPH::CharacterVirtual(&settings, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), userData, m_PhysicsSystem);
	}

	JoltCharacterController::~JoltCharacterController()
	{
		delete m_Character;
		m_Character = nullptr;
	}

	void JoltCharacterController::Move(const glm::vec3 &displacement, float deltaTime)
	{
		if (m_Character && m_PhysicsSystem && m_TempAllocator)
		{
			m_Character->SetLinearVelocity(GlmToJoltVec3(displacement));

			JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
			m_Character->ExtendedUpdate(
				deltaTime,
				m_PhysicsSystem->GetGravity(),
				updateSettings,
				m_PhysicsSystem->GetDefaultBroadPhaseLayerFilter(1),
				m_PhysicsSystem->GetDefaultLayerFilter(1),
				{},
				{},
				*m_TempAllocator
			);
		}
	}

	void JoltCharacterController::SetPosition(const glm::vec3 &position)
	{
		if (m_Character)
		{
			m_Character->SetPosition(GlmToJoltVec3(position));
		}
	}

	glm::vec3 JoltCharacterController::GetPosition() const
	{
		return m_Character ? JoltToGlmVec3(m_Character->GetPosition()) : glm::vec3(0.0f);
	}

	void JoltCharacterController::SetLinearVelocity(const glm::vec3 &velocity)
	{
		if (m_Character)
		{
			m_Character->SetLinearVelocity(GlmToJoltVec3(velocity));
		}
	}

	glm::vec3 JoltCharacterController::GetLinearVelocity() const
	{
		return m_Character ? JoltToGlmVec3(m_Character->GetLinearVelocity()) : glm::vec3(0.0f);
	}

	bool JoltCharacterController::IsOnGround() const
	{
		return m_Character ? m_Character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround : false;
	}

	glm::vec3 JoltCharacterController::GetGroundNormal() const
	{
		return m_Character ? JoltToGlmVec3(m_Character->GetGroundNormal()) : glm::vec3(0.0f, 1.0f, 0.0f);
	}

	void JoltCharacterController::SetRotation(const glm::quat &rotation)
	{
		if (m_Character)
		{
			m_Character->SetRotation(GlmToJoltQuat(rotation));
		}
	}

	glm::quat JoltCharacterController::GetRotation() const
	{
		return m_Character ? JoltToGlmQuat(m_Character->GetRotation()) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	void JoltCharacterController::SetRotationEuler(const glm::vec3 &eulerRot)
	{
		if (m_Character)
		{
			m_Character->SetRotation(GlmToJoltQuat(eulerRot));
		}
	}
}
