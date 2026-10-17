// Copyright (c) 2026 Evangelion Manuhutu

#include "jolt_mesh_collider.hpp"

#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>

#include "ignite/core/logger.hpp"

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

		auto buildMeshShape = [&]() -> JPH::ShapeRefC
		{
			JPH::TriangleList triangles;
			if (!desc.indices.empty())
			{
				triangles.reserve(desc.indices.size() / 3);
				for (size_t i = 0; i + 2 < desc.indices.size(); i += 3)
				{
					const uint32_t i0 = desc.indices[i];
					const uint32_t i1 = desc.indices[i + 1];
					const uint32_t i2 = desc.indices[i + 2];
					if (i0 >= joltVerts.size() || i1 >= joltVerts.size() || i2 >= joltVerts.size())
						continue;

					const JPH::Vec3 &v0 = joltVerts[i0];
					const JPH::Vec3 &v1 = joltVerts[i1];
					const JPH::Vec3 &v2 = joltVerts[i2];
					if ((v1 - v0).Cross(v2 - v0).LengthSq() <= 1e-12f)
						continue;

					triangles.push_back(JPH::Triangle(v0, v1, v2));
				}
			}
			else
			{
				triangles.reserve(joltVerts.size() / 3);
				for (size_t i = 0; i + 2 < joltVerts.size(); i += 3)
				{
					const JPH::Vec3 &v0 = joltVerts[i];
					const JPH::Vec3 &v1 = joltVerts[i + 1];
					const JPH::Vec3 &v2 = joltVerts[i + 2];
					if ((v1 - v0).Cross(v2 - v0).LengthSq() <= 1e-12f)
						continue;

					triangles.push_back(JPH::Triangle(v0, v1, v2));
				}
			}

			if (triangles.empty())
			{
				LOG_WARN("[Jolt Physics] MeshCollider has no valid non-degenerate triangles!");
				return nullptr;
			}

			JPH::MeshShapeSettings shapeSettings(triangles);
			auto result = shapeSettings.Create();
			if (!result.IsValid())
			{
				LOG_WARN("[Jolt Physics] Failed to create Mesh shape: {}", result.GetError().c_str());
				return nullptr;
			}
			return result.Get();
		};

		JPH::ShapeRefC innerShape;
		if (m_IsConvex)
		{
			JPH::ConvexHullShapeSettings shapeSettings(joltVerts.data(), static_cast<int>(joltVerts.size()));
			auto result = shapeSettings.Create();
			if (result.IsValid())
			{
				innerShape = result.Get();
			}
			else
			{
				LOG_WARN("[Jolt Physics] ConvexHullShape creation failed ({}), falling back to MeshShape", result.GetError().c_str());
				innerShape = buildMeshShape();
			}
		}
		else
		{
			innerShape = buildMeshShape();
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
			bounds.Translate(m_Shape->GetCenterOfMass());
			outAABB.min = glm::vec3(bounds.mMin.GetX(), bounds.mMin.GetY(), bounds.mMin.GetZ());
			outAABB.max = glm::vec3(bounds.mMax.GetX(), bounds.mMax.GetY(), bounds.mMax.GetZ());
		}
	}

	bool JoltMeshCollider::CastRay(const Ray &ray, RaycastHit &out, float maxDistance)
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
