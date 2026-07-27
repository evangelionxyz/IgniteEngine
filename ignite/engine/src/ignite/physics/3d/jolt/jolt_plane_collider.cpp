// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_plane_collider.hpp"
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ignite::physics
{
	static JPH::ShapeRefC CreatePlaneShape(const glm::vec3 &center, const glm::vec3 &scale)
	{
		JPH::PlaneShapeSettings planeSettings(JPH::Plane(JPH::Vec3(0.0f, 1.0f, 0.0f), 0.0f), nullptr, scale.x);
		auto planeResult = planeSettings.Create();
		if (!planeResult.IsValid())
			return nullptr;

		JPH::ShapeRefC innerShape = planeResult.Get();
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

	JoltPlaneCollider::JoltPlaneCollider(const PlaneColliderDesc &desc)
		: m_Center(desc.center)
		, m_Scale(desc.scale)
	{
		m_Shape = CreatePlaneShape(m_Center, m_Scale);
	}

	void JoltPlaneCollider::SetScale(const glm::vec3 &scale)
	{
		m_Scale = scale;
		m_Shape = CreatePlaneShape(m_Center, m_Scale);
	}

	void JoltPlaneCollider::SetCenter(const glm::vec3 &center)
	{
		m_Center = center;
		m_Shape = CreatePlaneShape(m_Center, m_Scale);
	}

	void JoltPlaneCollider::CalculateAABB(AABB &outAABB)
	{
		if (m_Shape)
		{
			JPH::AABox bounds = m_Shape->GetLocalBounds();
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
	}

	bool JoltPlaneCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
	{
		return false;
	}
}
