// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_mesh_collider.hpp"
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ignite::physics
{
	JoltMeshCollider::JoltMeshCollider(const MeshColliderDesc &desc)
		: m_Center(desc.center)
		, m_IsConvex(desc.isConvex)
	{
		if (desc.vertices.empty()) return;

		std::vector<JPH::Vec3> joltVerts;
		joltVerts.reserve(desc.vertices.size());
		for (const auto &v : desc.vertices)
		{
			joltVerts.push_back({ v.x, v.y, v.z });
		}

		JPH::ShapeRefC innerShape;
		if (m_IsConvex)
		{
			JPH::ConvexHullShapeSettings shapeSettings(joltVerts.data(), static_cast<int>(joltVerts.size()));
			auto result = shapeSettings.Create();
			if (result.IsValid())
			{
				innerShape = result.Get();
			}
		}
		else
		{
			JPH::TriangleList triangles;
			triangles.reserve(desc.indices.size() / 3);
			for (size_t i = 0; i + 2 < desc.indices.size(); i += 3)
			{
				triangles.push_back(JPH::Triangle(
					joltVerts[desc.indices[i]],
					joltVerts[desc.indices[i + 1]],
					joltVerts[desc.indices[i + 2]]
				));
			}

			JPH::MeshShapeSettings shapeSettings(triangles);
			auto result = shapeSettings.Create();
			if (result.IsValid())
			{
				innerShape = result.Get();
			}
		}

		if (innerShape && m_Center != glm::vec3(0.0f))
		{
			JPH::RotatedTranslatedShapeSettings offsetSettings(
				JPH::Vec3(m_Center.x, m_Center.y, m_Center.z),
				JPH::Quat::sIdentity(),
				innerShape
			);
			auto result = offsetSettings.Create();
			m_Shape = result.IsValid() ? result.Get() : innerShape;
		}
		else
		{
			m_Shape = innerShape;
		}
	}

	void JoltMeshCollider::CalculateAABB(AABB &outAABB)
	{
		if (m_Shape)
		{
			JPH::AABox bounds = m_Shape->GetLocalBounds();
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
	}

	bool JoltMeshCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
	{
		return false;
	}
}
