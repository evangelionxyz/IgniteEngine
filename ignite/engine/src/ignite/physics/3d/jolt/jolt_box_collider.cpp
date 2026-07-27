// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_box_collider.hpp"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ignite::physics
{
	static JPH::ShapeRefC CreateBoxShape(const glm::vec3 &center, const glm::vec3 &halfExtents)
	{
		JPH::BoxShapeSettings boxSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
		auto boxResult = boxSettings.Create();
		if (!boxResult.IsValid())
			return nullptr;

		JPH::ShapeRefC innerShape = boxResult.Get();
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

	JoltBoxCollider::JoltBoxCollider(const BoxColliderDesc &desc)
		: m_Center(desc.center)
		, m_HalfExtents(desc.halfExtents)
	{
		m_Shape = CreateBoxShape(m_Center, m_HalfExtents);
	}

	void JoltBoxCollider::SetHalfExtents(const glm::vec3 &halfExtents)
	{
		m_HalfExtents = halfExtents;
		m_Shape = CreateBoxShape(m_Center, m_HalfExtents);
	}

	void JoltBoxCollider::SetCenter(const glm::vec3 &center)
	{
		m_Center = center;
		m_Shape = CreateBoxShape(m_Center, m_HalfExtents);
	}

	void JoltBoxCollider::CalculateAABB(AABB &outAABB)
	{
		if (m_Shape)
		{
			JPH::AABox bounds = m_Shape->GetLocalBounds();
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
		else
		{
			outAABB.min = -m_HalfExtents;
			outAABB.max = m_HalfExtents;
		}
	}

	bool JoltBoxCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
	{
		// Shape local raycast
		return false;
	}
}
