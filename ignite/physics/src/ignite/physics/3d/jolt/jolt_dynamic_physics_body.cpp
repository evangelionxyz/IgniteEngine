// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_dynamic_physics_body.hpp"
#include "jolt_box_collider.hpp"
#include "jolt_sphere_collider.hpp"
#include "jolt_capsule_collider.hpp"
#include "jolt_plane_collider.hpp"
#include "jolt_mesh_collider.hpp"
#include "jolt_height_field_collider.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace ignite::physics
{
	static JPH::ShapeRefC ResolveJoltShape(const Ref<PhysicsCollider> &collider)
	{
		if (!collider) return nullptr;
		switch (collider->GetColliderType())
		{
		case ColliderType::Box:
			return std::static_pointer_cast<JoltBoxCollider>(collider)->GetJoltShape();
		case ColliderType::Sphere:
			return std::static_pointer_cast<JoltSphereCollider>(collider)->GetJoltShape();
		case ColliderType::Capsule:
			return std::static_pointer_cast<JoltCapsuleCollider>(collider)->GetJoltShape();
		case ColliderType::Plane:
			return std::static_pointer_cast<JoltPlaneCollider>(collider)->GetJoltShape();
		case ColliderType::Mesh:
			return std::static_pointer_cast<JoltMeshCollider>(collider)->GetJoltShape();
		case ColliderType::HeightField:
			return std::static_pointer_cast<JoltHeightFieldCollider>(collider)->GetJoltShape();
		}
		return nullptr;
	}

	static inline JPH::Vec3 GlmToJoltVec3(const glm::vec3 &v)
	{
		return { v.x, v.y, v.z };
	}

	static inline glm::vec3 JoltToGlmVec3(const JPH::Vec3 &v)
	{
		return { v.GetX(), v.GetY(), v.GetZ() };
	}

	static inline JPH::Quat GlmToJoltQuat(const glm::quat &q)
	{
		return { q.x, q.y, q.z, q.w };
	}

	static inline glm::quat JoltToGlmQuat(const JPH::Quat &q)
	{
		return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
	}

	JoltDynamicPhysicsBody::JoltDynamicPhysicsBody(uint32_t bodyId, JPH::BodyInterface *bodyInterface)
		: m_BodyId(bodyId)
		, m_BodyInterface(bodyInterface)
	{
	}

	void JoltDynamicPhysicsBody::AddForce(const glm::vec3 &force)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddForce(JPH::BodyID(m_BodyId), GlmToJoltVec3(force));
	}

	void JoltDynamicPhysicsBody::AddForceAtPosition(const glm::vec3 &force, const glm::vec3 &position)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddForce(JPH::BodyID(m_BodyId), GlmToJoltVec3(force), GlmToJoltVec3(position));
	}

	void JoltDynamicPhysicsBody::AddTorque(const glm::vec3 &torque)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddTorque(JPH::BodyID(m_BodyId), GlmToJoltVec3(torque));
	}

	void JoltDynamicPhysicsBody::AddForceAndTorque(const glm::vec3 &force, const glm::vec3 &torque)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddForceAndTorque(JPH::BodyID(m_BodyId), GlmToJoltVec3(force), GlmToJoltVec3(torque));
	}

	void JoltDynamicPhysicsBody::AddAngularImpulse(const glm::vec3 &impulse)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddAngularImpulse(JPH::BodyID(m_BodyId), GlmToJoltVec3(impulse));
	}

	void JoltDynamicPhysicsBody::AddImpulse(const glm::vec3 &impulse)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddImpulse(JPH::BodyID(m_BodyId), GlmToJoltVec3(impulse));
	}

	void JoltDynamicPhysicsBody::AddImpulseAtPosition(const glm::vec3 &impulse, const glm::vec3 &position)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddImpulse(JPH::BodyID(m_BodyId), GlmToJoltVec3(impulse), GlmToJoltVec3(position));
	}

	void JoltDynamicPhysicsBody::AddLinearVelocity(const glm::vec3 &velocity)
	{
		if (m_BodyInterface)
			m_BodyInterface->AddLinearVelocity(JPH::BodyID(m_BodyId), GlmToJoltVec3(velocity));
	}

	void JoltDynamicPhysicsBody::SetGravityFactor(float value)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetGravityFactor(JPH::BodyID(m_BodyId), value);
	}

	void JoltDynamicPhysicsBody::SetMaxLinearVelocity(float max)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetMaxLinearVelocity(JPH::BodyID(m_BodyId), max);
	}

	void JoltDynamicPhysicsBody::SetMaxAngularVelocity(float max)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetMaxAngularVelocity(JPH::BodyID(m_BodyId), max);
	}

	void JoltDynamicPhysicsBody::SetLinearDamping(float damping)
	{
		// Damping configuration
	}

	void JoltDynamicPhysicsBody::SetAngularDamping(float damping)
	{
		// Damping configuration
	}

	void JoltDynamicPhysicsBody::SetFriction(float value)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetFriction(JPH::BodyID(m_BodyId), value);
	}

	void JoltDynamicPhysicsBody::SetRestitution(float value)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetRestitution(JPH::BodyID(m_BodyId), value);
	}

	void JoltDynamicPhysicsBody::SetMotionType(BodyType bodyType)
	{
		if (!m_BodyInterface) return;
		JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
		if (bodyType == BodyType::Static) motionType = JPH::EMotionType::Static;
		else if (bodyType == BodyType::Kinematic) motionType = JPH::EMotionType::Kinematic;
		m_BodyInterface->SetMotionType(JPH::BodyID(m_BodyId), motionType, JPH::EActivation::Activate);
	}

	void JoltDynamicPhysicsBody::SetCollider(const Ref<PhysicsCollider> &collider, bool activate)
	{
		if (!m_BodyInterface || m_BodyId == JPH::BodyID::cInvalidBodyID)
			return;

		JPH::ShapeRefC shape = ResolveJoltShape(collider);
		if (shape)
		{
			m_BodyInterface->SetShape(JPH::BodyID(m_BodyId), shape, true, activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
		}
	}

	void JoltDynamicPhysicsBody::ActivateBody()
	{
		if (m_BodyInterface)
			m_BodyInterface->ActivateBody(JPH::BodyID(m_BodyId));
	}

	void JoltDynamicPhysicsBody::DeactivateBody()
	{
		if (m_BodyInterface)
			m_BodyInterface->DeactivateBody(JPH::BodyID(m_BodyId));
	}

	void JoltDynamicPhysicsBody::DestroyBody()
	{
		if (m_BodyInterface && m_BodyId != JPH::BodyID::cInvalidBodyID)
		{
			m_BodyInterface->RemoveBody(JPH::BodyID(m_BodyId));
			m_BodyInterface->DestroyBody(JPH::BodyID(m_BodyId));
			m_BodyId = JPH::BodyID::cInvalidBodyID;
		}
	}

	bool JoltDynamicPhysicsBody::IsActive()
	{
		return m_BodyInterface ? m_BodyInterface->IsActive(JPH::BodyID(m_BodyId)) : false;
	}

	void JoltDynamicPhysicsBody::SetPosition(const glm::vec3 &position, bool activate)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetPosition(JPH::BodyID(m_BodyId), GlmToJoltVec3(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltDynamicPhysicsBody::SetEulerAngleRotation(const glm::vec3 &rotation, bool activate)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetRotation(JPH::BodyID(m_BodyId), GlmToJoltQuat(glm::quat(rotation)), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltDynamicPhysicsBody::SetRotation(const glm::quat &rotation, bool activate)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetRotation(JPH::BodyID(m_BodyId), GlmToJoltQuat(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltDynamicPhysicsBody::SetLinearVelocity(const glm::vec3 &vel)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetLinearVelocity(JPH::BodyID(m_BodyId), GlmToJoltVec3(vel));
	}

	void JoltDynamicPhysicsBody::SetAngularVelocity(const glm::vec3 &vel)
	{
		if (m_BodyInterface)
			m_BodyInterface->SetAngularVelocity(JPH::BodyID(m_BodyId), GlmToJoltVec3(vel));
	}

	float JoltDynamicPhysicsBody::GetRestitution()
	{
		return m_BodyInterface ? m_BodyInterface->GetRestitution(JPH::BodyID(m_BodyId)) : 0.0f;
	}

	float JoltDynamicPhysicsBody::GetFriction()
	{
		return m_BodyInterface ? m_BodyInterface->GetFriction(JPH::BodyID(m_BodyId)) : 0.0f;
	}

	float JoltDynamicPhysicsBody::GetGravityFactor()
	{
		return m_BodyInterface ? m_BodyInterface->GetGravityFactor(JPH::BodyID(m_BodyId)) : 1.0f;
	}

	glm::vec3 JoltDynamicPhysicsBody::GetPosition()
	{
		return m_BodyInterface ? JoltToGlmVec3(m_BodyInterface->GetPosition(JPH::BodyID(m_BodyId))) : glm::vec3(0.0f);
	}

	glm::vec3 JoltDynamicPhysicsBody::GetEulerAngles()
	{
		return m_BodyInterface ? glm::eulerAngles(JoltToGlmQuat(m_BodyInterface->GetRotation(JPH::BodyID(m_BodyId)))) : glm::vec3(0.0f);
	}

	glm::quat JoltDynamicPhysicsBody::GetRotation()
	{
		return m_BodyInterface ? JoltToGlmQuat(m_BodyInterface->GetRotation(JPH::BodyID(m_BodyId))) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	glm::vec3 JoltDynamicPhysicsBody::GetCenterOfMassPosition()
	{
		return m_BodyInterface ? JoltToGlmVec3(m_BodyInterface->GetCenterOfMassPosition(JPH::BodyID(m_BodyId))) : glm::vec3(0.0f);
	}

	glm::vec3 JoltDynamicPhysicsBody::GetLinearVelocity()
	{
		return m_BodyInterface ? JoltToGlmVec3(m_BodyInterface->GetLinearVelocity(JPH::BodyID(m_BodyId))) : glm::vec3(0.0f);
	}

	glm::vec3 JoltDynamicPhysicsBody::GetAngularVelocity()
	{
		return m_BodyInterface ? JoltToGlmVec3(m_BodyInterface->GetAngularVelocity(JPH::BodyID(m_BodyId))) : glm::vec3(0.0f);
	}

	uint64_t JoltDynamicPhysicsBody::GetUserData()
	{
		return m_BodyInterface ? m_BodyInterface->GetUserData(JPH::BodyID(m_BodyId)) : 0;
	}
}
