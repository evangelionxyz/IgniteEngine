// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_SPHERE_COLLIDER_HPP
#define IGN_JOLT_SPHERE_COLLIDER_HPP

#include "ignite/physics/3d/physics_collider.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace ignite::physics
{
	class IGN_PHYSICS_API JoltSphereCollider : public PhysicsSphereCollider
	{
	public:
		explicit JoltSphereCollider(const SphereColliderDesc &desc);
		virtual ~JoltSphereCollider() override = default;

		virtual void CalculateAABB(AABB &outAABB) override;
		virtual bool CastRay(const Ray &ray, RaycastHit &out, float maxDistance = 100.0f) override;

		virtual void SetRadius(float radius) override;
		virtual float GetRadius() const override { return m_Radius; }

		virtual void SetCenter(const glm::vec3 &center) override;
		virtual glm::vec3 GetCenter() const override { return m_Center; }

		JPH::ShapeRefC GetJoltShape() const { return m_Shape; }

	private:
		glm::vec3 m_Center{ 0.0f };
		float m_Radius;
		JPH::ShapeRefC m_Shape;
	};
}

#endif
