// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_static_physics_body.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyID.h>

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

	static glm::quat JoltToGlmQuat(const JPH::Quat &q)
	{
		return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
	}

	JoltStaticPhysicsBody::JoltStaticPhysicsBody(uint32_t bodyId, JPH::BodyInterface *bodyInterface)
		: m_BodyId(bodyId)
		, m_BodyInterface(bodyInterface)
	{
	}

	void JoltStaticPhysicsBody::ActivateBody()
	{
		if (m_BodyInterface)
			m_BodyInterface->ActivateBody(JPH::BodyID(m_BodyId));
	}

	void JoltStaticPhysicsBody::DeactivateBody()
	{
		if (m_BodyInterface)
			m_BodyInterface->DeactivateBody(JPH::BodyID(m_BodyId));
	}

	void JoltStaticPhysicsBody::DestroyBody()
	{
		if (m_BodyInterface && m_BodyId != JPH::BodyID::cInvalidBodyID)
		{
			m_BodyInterface->RemoveBody(JPH::BodyID(m_BodyId));
			m_BodyInterface->DestroyBody(JPH::BodyID(m_BodyId));
			m_BodyId = JPH::BodyID::cInvalidBodyID;
		}
	}

	bool JoltStaticPhysicsBody::IsActive()
	{
		return m_BodyInterface ? m_BodyInterface->IsActive(JPH::BodyID(m_BodyId)) : false;
	}

	void JoltStaticPhysicsBody::SetPosition(const glm::vec3 &position, bool activate)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetPosition(JPH::BodyID(m_BodyId), GlmToJoltVec3(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltStaticPhysicsBody::SetEulerAngleRotation(const glm::vec3 &rotation, bool activate)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetRotation(JPH::BodyID(m_BodyId), GlmToJoltQuat(glm::quat(rotation)), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltStaticPhysicsBody::SetRotation(const glm::quat &rotation, bool activate)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetRotation(JPH::BodyID(m_BodyId), GlmToJoltQuat(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltStaticPhysicsBody::SetFriction(float value)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetFriction(JPH::BodyID(m_BodyId), value);
	}

	void JoltStaticPhysicsBody::SetRestitution(float value)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetRestitution(JPH::BodyID(m_BodyId), value);
	}

	float JoltStaticPhysicsBody::GetRestitution()
	{
		return m_BodyInterface ? m_BodyInterface->GetRestitution(JPH::BodyID(m_BodyId)) : 0.0f;
	}

	float JoltStaticPhysicsBody::GetFriction()
	{
		return m_BodyInterface ? m_BodyInterface->GetFriction(JPH::BodyID(m_BodyId)) : 0.0f;
	}

	glm::vec3 JoltStaticPhysicsBody::GetPosition()
	{
		return m_BodyInterface ? JoltToGlmVec3(m_BodyInterface->GetPosition(JPH::BodyID(m_BodyId))) : glm::vec3(0.0f);
	}

	glm::vec3 JoltStaticPhysicsBody::GetEulerAngles()
	{
		return m_BodyInterface ? glm::eulerAngles(JoltToGlmQuat(m_BodyInterface->GetRotation(JPH::BodyID(m_BodyId)))) : glm::vec3(0.0f);
	}

	glm::quat JoltStaticPhysicsBody::GetRotation()
	{
		return m_BodyInterface ? JoltToGlmQuat(m_BodyInterface->GetRotation(JPH::BodyID(m_BodyId))) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	uint64_t JoltStaticPhysicsBody::GetUserData()
	{
		return m_BodyInterface ? m_BodyInterface->GetUserData(JPH::BodyID(m_BodyId)) : 0;
	}
}
