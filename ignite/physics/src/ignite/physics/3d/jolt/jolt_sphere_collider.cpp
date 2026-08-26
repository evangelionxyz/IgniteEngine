// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_sphere_collider.hpp"
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

#include "ignite/core/logger.hpp"

namespace ignite::physics
{
	static JPH::ShapeRefC CreateSphereShape(const glm::vec3 &center, float radius)
	{
		JPH::SphereShapeSettings sphereSettings(radius);
		auto sphereResult = sphereSettings.Create();
        if (!sphereResult.IsValid())
        {
            LOG_ASSERT(false, "[Jolt Physics] Failed to create Sphere Shape!");
			return nullptr;
        }

		JPH::ShapeRefC innerShape = sphereResult.Get();
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

	JoltSphereCollider::JoltSphereCollider(const SphereColliderDesc &desc)
		: m_Center(desc.center)
		, m_Radius(desc.radius)
	{
		m_Shape = CreateSphereShape(m_Center, m_Radius);
	}

	void JoltSphereCollider::SetRadius(float radius)
	{
		m_Radius = radius;
		m_Shape = CreateSphereShape(m_Center, m_Radius);
	}

	void JoltSphereCollider::SetCenter(const glm::vec3 &center)
	{
		m_Center = center;
		m_Shape = CreateSphereShape(m_Center, m_Radius);
	}

	void JoltSphereCollider::CalculateAABB(AABB &outAABB)
	{
		if (m_Shape)
		{
			JPH::AABox bounds = m_Shape->GetLocalBounds();
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
		else
		{
			outAABB.min = glm::vec3(-m_Radius);
			outAABB.max = glm::vec3(m_Radius);
		}
	}

	bool JoltSphereCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
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
