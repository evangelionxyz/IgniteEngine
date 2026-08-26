// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_HEIGHT_FIELD_COLLIDER_HPP
#define IGN_JOLT_HEIGHT_FIELD_COLLIDER_HPP

#include "ignite/physics/3d/physics_collider.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <vector>

namespace ignite::physics
{
	class IGN_PHYSICS_API JoltHeightFieldCollider : public PhysicsHeightFieldCollider
	{
	public:
		explicit JoltHeightFieldCollider(const HeightFieldColliderDesc &desc);
		virtual ~JoltHeightFieldCollider() override = default;

		virtual void CalculateAABB(AABB &outAABB) override;
		virtual bool CastRay(const Ray &ray, RaycastHit &out, float maxDistance = 100.0f) override;

		virtual uint32_t GetSampleCount() const override { return m_SampleCount; }
		virtual const std::vector<float> &GetHeights() const override { return m_Heights; }
		virtual glm::vec3 GetScale() const override { return m_Scale; }

		virtual void SetCenter(const glm::vec3 &center) override { m_Center = center; }
		virtual glm::vec3 GetCenter() const override { return m_Center; }

		JPH::ShapeRefC GetJoltShape() const { return m_Shape; }

	private:
		glm::vec3 m_Center{ 0.0f };
		glm::vec3 m_Scale{ 1.0f };
		uint32_t m_SampleCount{ 0 };
		std::vector<float> m_Heights;
		JPH::ShapeRefC m_Shape;
	};
}

#endif
