// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_sphere_collider.hpp"
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ignite::physics
{
	static JPH::ShapeRefC CreateSphereShape(const glm::vec3 &center, float radius)
	{
		JPH::SphereShapeSettings sphereSettings(radius);
		auto sphereResult = sphereSettings.Create();
		if (!sphereResult.IsValid())
			return nullptr;

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
		return false;
	}
}
