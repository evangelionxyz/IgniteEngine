// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_COLLIDER_HPP
#define IGN_PHYSICS_COLLIDER_HPP

#include "ignite/physics/physics_types.hpp"

#include "ignite/math/aabb.hpp"
#include "ignite/math/ray.hpp"

namespace ignite::physics
{
	class IGN_PHYSICS_API PhysicsCollider
    {
    public:
        virtual ~PhysicsCollider() = default;

        virtual ColliderType GetColliderType() const = 0;
        virtual void CalculateAABB(AABB &outAABB) = 0;
        virtual bool CastRay(const Ray &ray, RaycastHit &out, float maxDistance = 100.0f) = 0;
        virtual void SetCenter(const glm::vec3 &center) {}
        virtual glm::vec3 GetCenter() const { return glm::vec3(0.0f); }
    };

    class IGN_PHYSICS_API PhysicsBoxCollider : public PhysicsCollider
    {
    public:
        virtual ColliderType GetColliderType() const override { return ColliderType::Box; }
        virtual void SetHalfExtents(const glm::vec3 &halfExtents) = 0;
        virtual glm::vec3 GetHalfExtents() const = 0;
    };

    class IGN_PHYSICS_API PhysicsSphereCollider : public PhysicsCollider
    {
    public:
        virtual ColliderType GetColliderType() const override { return ColliderType::Sphere; }
        virtual void SetRadius(float radius) = 0;
        virtual float GetRadius() const = 0;
    };

    class IGN_PHYSICS_API PhysicsCapsuleCollider : public PhysicsCollider
    {
    public:
        virtual ColliderType GetColliderType() const override { return ColliderType::Capsule; }
        virtual void SetRadiusAndHeight(float radius, float height) = 0;
        virtual float GetRadius() const = 0;
        virtual float GetHeight() const = 0;
    };

    class IGN_PHYSICS_API PhysicsPlaneCollider : public PhysicsCollider
    {
    public:
        virtual ColliderType GetColliderType() const override { return ColliderType::Plane; }
        virtual void SetScale(const glm::vec3 &scale) = 0;
        virtual glm::vec3 GetScale() const = 0;
    };

    class IGN_PHYSICS_API PhysicsMeshCollider : public PhysicsCollider
    {
    public:
        virtual ColliderType GetColliderType() const override { return ColliderType::Mesh; }
        virtual bool IsConvex() const = 0;
    };

    class IGN_PHYSICS_API PhysicsHeightFieldCollider : public PhysicsCollider
	{
	public:
		virtual ColliderType GetColliderType() const override { return ColliderType::HeightField; }
		virtual uint32_t GetSampleCount() const = 0;
		virtual const std::vector<float> &GetHeights() const = 0;
		virtual glm::vec3 GetScale() const = 0;
	};
}

#endif
