// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_capsule_collider.hpp"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

#include "ignite/core/logger.hpp"

namespace ignite::physics
{
	static JPH::ShapeRefC CreateCapsuleShape(const glm::vec3 &center, float radius, float halfHeight)
	{
		JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
		auto capsuleResult = capsuleSettings.Create();
        if (!capsuleResult.IsValid())
        {
            LOG_ASSERT(false, "[Jolt Physics] Failed to create Capsule Shape!");
            return nullptr;
        }

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
		m_HalfHeight = glm::max(height * 0.5f - radius, 0.0f);
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
			bounds.Translate(m_Shape->GetCenterOfMass());
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
		else
		{
			outAABB.min = m_Center + glm::vec3(-m_Radius, -(m_HalfHeight + m_Radius), -m_Radius);
			outAABB.max = m_Center + glm::vec3(m_Radius, m_HalfHeight + m_Radius, m_Radius);
		}
	}

	bool JoltCapsuleCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
	{
		if (!m_Shape)
			return false;

		const JPH::Vec3 com = m_Shape->GetCenterOfMass();
		JPH::RayCast rayCast(
			JPH::Vec3(ray.origin.x, ray.origin.y, ray.origin.z) - com,
			JPH::Vec3(ray.direction.x * maxDistance, ray.direction.y * maxDistance, ray.direction.z * maxDistance)
		);

		JPH::SubShapeIDCreator subShapeIDCreator;
		JPH::RayCastResult hit;
		if (m_Shape->CastRay(rayCast, subShapeIDCreator, hit))
		{
			out.fraction = hit.mFraction;
			JPH::Vec3 hitPosOnShape = rayCast.GetPointOnRay(hit.mFraction);
			JPH::Vec3 hitPos = hitPosOnShape + com;
			out.hitPoint = glm::vec3(hitPos.GetX(), hitPos.GetY(), hitPos.GetZ());
			JPH::Vec3 normal = m_Shape->GetSurfaceNormal(hit.mSubShapeID2, hitPosOnShape);
			out.hitNormal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
			return true;
		}

		return false;
	}
}
