// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_height_field_collider.hpp"
#include "ignite/physics/physics_log.hpp"
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

namespace ignite::physics
{
	JoltHeightFieldCollider::JoltHeightFieldCollider(const HeightFieldColliderDesc &desc)
		: m_Center(desc.center)
		, m_Scale(desc.scale)
		, m_SampleCount(desc.sampleCount)
		, m_Heights(desc.heights)
	{
		if (m_Heights.empty() || m_SampleCount < 2 || m_Heights.size() != static_cast<size_t>(m_SampleCount) * static_cast<size_t>(m_SampleCount))
		{
			IGN_PHYSICS_ERROR("[JoltHeightFieldCollider] Invalid sample data or count for heightfield collider!");
			return;
		}

		JPH::HeightFieldShapeSettings shapeSettings(
			m_Heights.data(),
			JPH::Vec3(m_Center.x, m_Center.y, m_Center.z),
			JPH::Vec3(m_Scale.x, m_Scale.y, m_Scale.z),
			m_SampleCount
		);

		auto result = shapeSettings.Create();
		if (result.IsValid())
		{
			m_Shape = result.Get();
		}
		else
		{
			IGN_PHYSICS_ERROR("[JoltHeightFieldCollider] Failed to create Jolt HeightFieldShape: {}", result.GetError().c_str());
		}
	}

	void JoltHeightFieldCollider::CalculateAABB(AABB &outAABB)
	{
		if (m_Shape)
		{
			JPH::AABox bounds = m_Shape->GetLocalBounds();
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
	}

	bool JoltHeightFieldCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
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
