// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_box_collider.hpp"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

#include "ignite/core/logger.hpp"

namespace ignite::physics
{
	static JPH::ShapeRefC CreateBoxShape(const glm::vec3 &center, const glm::vec3 &halfExtents)
	{
		JPH::BoxShapeSettings boxSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
		auto boxResult = boxSettings.Create();
        if (!boxResult.IsValid())
        {
            LOG_ASSERT(false, "[Jolt Physics] Failed to create Box shape!");
			return nullptr;
        }

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
		if (!m_Shape)
			return false;

		JPH::RayCast rayCast(
			JPH::Vec3(ray.origin.x, ray.origin.y, ray.origin.z),
			JPH::Vec3(ray.direction.x * maxDistance, ray.direction.y * maxDistance, ray.direction.z * maxDistance)
		);

		JPH::SubShapeIDCreator subShapeIDCreator;
		JPH::RayCastResult hit;
		if (m_Shape->CastRay(rayCast, subShapeIDCreator, hit))
		{
			out.fraction = hit.mFraction;
			JPH::Vec3 hitPos = rayCast.GetPointOnRay(hit.mFraction);
			out.hitPoint = glm::vec3(hitPos.GetX(), hitPos.GetY(), hitPos.GetZ());
			JPH::Vec3 normal = m_Shape->GetSurfaceNormal(hit.mSubShapeID2, hitPos);
			out.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
			return true;
		}

		return false;
	}
}
