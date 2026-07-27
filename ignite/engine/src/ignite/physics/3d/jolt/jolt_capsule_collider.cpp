// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_capsule_collider.hpp"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ignite::physics
{
	static JPH::ShapeRefC CreateCapsuleShape(const glm::vec3 &center, float radius, float halfHeight)
	{
		JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
		auto capsuleResult = capsuleSettings.Create();
		if (!capsuleResult.IsValid())
			return nullptr;

		JPH::ShapeRefC innerShape = capsuleResult.Get();
		if (center != glm::vec3(0.0f))
		{
			JPH::RotatedTranslatedShapeSettings offsetSettings(
				JPH::Vec3(center.x, center.y, center.z),
				JPH::Quat::sIdentity(),
				innerShape
			);
			auto result = offsetSettings.Create();
			return result.IsValid() ? result.Get() : nullptr;
		}
		return innerShape;
	}

	JoltCapsuleCollider::JoltCapsuleCollider(const CapsuleColliderDesc &desc)
		: m_Center(desc.center)
		, m_Radius(desc.radius)
		, m_HalfHeight(desc.halfHeight)
	{
		m_Shape = CreateCapsuleShape(m_Center, m_Radius, m_HalfHeight);
	}

	void JoltCapsuleCollider::SetRadiusAndHeight(float radius, float height)
	{
		m_Radius = radius;
		m_HalfHeight = height * 0.5f;
		m_Shape = CreateCapsuleShape(m_Center, m_Radius, m_HalfHeight);
	}

	void JoltCapsuleCollider::SetCenter(const glm::vec3 &center)
	{
		m_Center = center;
		m_Shape = CreateCapsuleShape(m_Center, m_Radius, m_HalfHeight);
	}

	void JoltCapsuleCollider::CalculateAABB(AABB &outAABB)
	{
		if (m_Shape)
		{
			JPH::AABox bounds = m_Shape->GetLocalBounds();
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
		else
		{
			outAABB.min = glm::vec3(-m_Radius, -(m_HalfHeight + m_Radius), -m_Radius);
			outAABB.max = glm::vec3(m_Radius, m_HalfHeight + m_Radius, m_Radius);
		}
	}

	bool JoltCapsuleCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
	{
		return false;
	}
}
