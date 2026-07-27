// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_PHYSICS_3D_HPP
#define IGN_PHYSICS_3D_HPP

#include "ignite/physics/physics_types.hpp"
#include "physics_dynamic_actor.hpp"
#include "physics_static_actor.hpp"
#include "physics_collider.hpp"
#include "physics_character_controller.hpp"

namespace ignite::physics
{
	class IGN_API Physics3D
	{
	public:
		virtual ~Physics3D() = default;

		virtual void SimulationStart(const Physics3DSettings &settings) = 0;
		virtual void SimulationStop() = 0;
		virtual void Simulate(float deltaTime) = 0;

		virtual Ref<PhysicsDynamicActor> CreateDynamicBody(const RigidBodyDesc &desc, const PhysicsTransformData &transform, uint64_t userData, const Ref<PhysicsCollider> &collider = nullptr) = 0;
		virtual Ref<PhysicsStaticActor> CreateStaticBody(const RigidBodyDesc &desc, const PhysicsTransformData &transform, uint64_t userData, const Ref<PhysicsCollider> &collider = nullptr) = 0;

		virtual Ref<PhysicsBoxCollider> CreateBoxCollider(const BoxColliderDesc &desc) = 0;
		virtual Ref<PhysicsSphereCollider> CreateSphereCollider(const SphereColliderDesc &desc) = 0;
		virtual Ref<PhysicsCapsuleCollider> CreateCapsuleCollider(const CapsuleColliderDesc &desc) = 0;
		virtual Ref<PhysicsPlaneCollider> CreatePlaneCollider(const PlaneColliderDesc &desc) = 0;
		virtual Ref<PhysicsMeshCollider> CreateMeshCollider(const MeshColliderDesc &desc) = 0;

		virtual Ref<PhysicsCharacterController> CreateCharacterController(const CharacterControllerDesc &desc, uint64_t userData) = 0;

		virtual void DestroyDynamicBody(Ref<PhysicsDynamicActor> body) = 0;
		virtual void DestroyStaticBody(Ref<PhysicsStaticActor> body) = 0;
		virtual void DestroyCollider(Ref<PhysicsCollider> collider) = 0;
		virtual void DestroyCharacterController(Ref<PhysicsCharacterController> character) = 0;

		virtual bool Raycast(const Ray &ray, RaycastHit &outHit, float maxDistance = 1000.0f) = 0;

		virtual std::vector<CollisionEvent> DrainCollisionEvents() = 0;
		virtual std::vector<BodyActivationEvent> DrainActivationEvents() = 0;

		static Scope<Physics3D> Create(Physics3DType type);
	};
}

#endif