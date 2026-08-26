// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_BOX_COLLIDER_HPP
#define IGN_JOLT_BOX_COLLIDER_HPP

#include "ignite/physics/3d/physics_collider.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace ignite::physics
{
	class IGN_PHYSICS_API JoltBoxCollider : public PhysicsBoxCollider
	{
	public:
		explicit JoltBoxCollider(const BoxColliderDesc &desc);
		virtual ~JoltBoxCollider() override = default;

		virtual void CalculateAABB(AABB &outAABB) override;
		virtual bool CastRay(const Ray &ray, RaycastHit &out, float maxDistance = 100.0f) override;

		virtual void SetHalfExtents(const glm::vec3 &halfExtents) override;
		virtual glm::vec3 GetHalfExtents() const override { return m_HalfExtents; }

		virtual void SetCenter(const glm::vec3 &center) override;
		virtual glm::vec3 GetCenter() const override { return m_Center; }

		JPH::ShapeRefC GetJoltShape() const { return m_Shape; }

	private:
		glm::vec3 m_Center{ 0.0f };
		glm::vec3 m_HalfExtents;
		JPH::ShapeRefC m_Shape;
	};
}

#endif
