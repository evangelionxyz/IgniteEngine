// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_JOLT_PHYSICS_HPP
#define IGN_JOLT_PHYSICS_HPP

#include "ignite/physics/physics_log.hpp"
#include "ignite/physics/3d/physics_3d.hpp"
#include "jolt_dynamic_physics_body.hpp"
#include "jolt_static_physics_body.hpp"
#include "jolt_box_collider.hpp"
#include "jolt_sphere_collider.hpp"
#include "jolt_capsule_collider.hpp"
#include "jolt_mesh_collider.hpp"
#include "jolt_plane_collider.hpp"
#include "jolt_height_field_collider.hpp"
#include "jolt_character_controller.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <mutex>
#include <array>
#include <vector>

namespace ignite::physics
{
	namespace Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = MAX_PHYSICS_LAYERS;
	}

	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr uint32_t NUM_LAYERS(2);
	}

	class JoltObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		const Physics3DSettings *settings = nullptr;

		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
		{
			if (settings)
			{
				return settings->CanLayersCollide(static_cast<uint32_t>(inObject1), static_cast<uint32_t>(inObject2));
			}
			return true;
		}
	};

	class JoltBroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JoltBroadPhaseLayerInterfaceImpl()
		{
			for (uint32_t i = 0; i < MAX_PHYSICS_LAYERS; ++i)
			{
				m_ObjectToBP[i] = BroadPhaseLayers::MOVING;
			}
		}

		virtual uint32_t GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
		{
			if (inLayer < MAX_PHYSICS_LAYERS)
				return m_ObjectToBP[inLayer];
			return BroadPhaseLayers::MOVING;
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
		{
			switch ((JPH::BroadPhaseLayer::Type)inLayer)
			{
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
			default:														return "INVALID";
			}
		}
#endif

	private:
		JPH::BroadPhaseLayer m_ObjectToBP[MAX_PHYSICS_LAYERS];
	};

	class JoltObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			return true;
		}
	};

	class JoltContactListener : public JPH::ContactListener
	{
	public:
		virtual JPH::ValidateResult OnContactValidate(const JPH::Body &bodyA, const JPH::Body &bodyB, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override
		{
			return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		virtual void OnContactAdded(const JPH::Body &bodyA, const JPH::Body &bodyB, const JPH::ContactManifold &manifold, JPH::ContactSettings &ioSettings) override
		{
			CollisionEvent ev;
			ev.type = CollisionEventType::Enter;
			ev.userDataA = bodyA.GetUserData();
			ev.userDataB = bodyB.GetUserData();
			JPH::Vec3 pt = manifold.GetWorldSpaceContactPointOn1(0);
			ev.contactPoint = { pt.GetX(), pt.GetY(), pt.GetZ() };
			ev.contactNormal = { manifold.mWorldSpaceNormal.GetX(), manifold.mWorldSpaceNormal.GetY(), manifold.mWorldSpaceNormal.GetZ() };
			std::lock_guard<std::mutex> lock(m_EventMutex);
			m_PendingEvents.push_back(ev);
		}

		virtual void OnContactPersisted(const JPH::Body &bodyA, const JPH::Body &bodyB, const JPH::ContactManifold &manifold, JPH::ContactSettings &ioSettings) override
		{
			CollisionEvent ev;
			ev.type = CollisionEventType::Stay;
			ev.userDataA = bodyA.GetUserData();
			ev.userDataB = bodyB.GetUserData();
			JPH::Vec3 pt = manifold.GetWorldSpaceContactPointOn1(0);
			ev.contactPoint = { pt.GetX(), pt.GetY(), pt.GetZ() };
			ev.contactNormal = { manifold.mWorldSpaceNormal.GetX(), manifold.mWorldSpaceNormal.GetY(), manifold.mWorldSpaceNormal.GetZ() };
			std::lock_guard<std::mutex> lock(m_EventMutex);
			m_PendingEvents.push_back(ev);
		}

		virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
		{
			CollisionEvent ev;
			ev.type = CollisionEventType::Exit;
			std::lock_guard<std::mutex> lock(m_EventMutex);
			m_PendingEvents.push_back(ev);
		}

		std::vector<CollisionEvent> DrainEvents()
		{
			std::lock_guard<std::mutex> lock(m_EventMutex);
			std::vector<CollisionEvent> result;
			result.swap(m_PendingEvents);
			return result;
		}

	private:
		std::mutex m_EventMutex;
		std::vector<CollisionEvent> m_PendingEvents;
	};

	class JoltBodyActivationListener : public JPH::BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData) override
		{
			BodyActivationEvent ev;
			ev.type = ActivationEventType::Activated;
			ev.userData = inBodyUserData;
			std::lock_guard<std::mutex> lock(m_EventMutex);
			m_PendingEvents.push_back(ev);
		}

		virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData) override
		{
			BodyActivationEvent ev;
			ev.type = ActivationEventType::Deactivated;
			ev.userData = inBodyUserData;
			std::lock_guard<std::mutex> lock(m_EventMutex);
			m_PendingEvents.push_back(ev);
		}

		std::vector<BodyActivationEvent> DrainEvents()
		{
			std::lock_guard<std::mutex> lock(m_EventMutex);
			std::vector<BodyActivationEvent> result;
			result.swap(m_PendingEvents);
			return result;
		}

	private:
		std::mutex m_EventMutex;
		std::vector<BodyActivationEvent> m_PendingEvents;
	};

	class IGN_API JoltPhysics : public Physics3D
	{
	public:
		JoltPhysics();
		~JoltPhysics();

		virtual void SimulationStart(const Physics3DSettings &settings) override;
		virtual void SimulationStop() override;
		virtual void Simulate(float deltaTime) override;

		virtual Ref<PhysicsDynamicActor> CreateDynamicBody(const RigidBodyDesc &desc, const PhysicsTransformData &transform, uint64_t userData, const Ref<PhysicsCollider> &collider = nullptr) override;
		virtual Ref<PhysicsStaticActor> CreateStaticBody(const RigidBodyDesc &desc, const PhysicsTransformData &transform, uint64_t userData, const Ref<PhysicsCollider> &collider = nullptr) override;

		virtual Ref<PhysicsBoxCollider> CreateBoxCollider(const BoxColliderDesc &desc) override;
		virtual Ref<PhysicsSphereCollider> CreateSphereCollider(const SphereColliderDesc &desc) override;
		virtual Ref<PhysicsCapsuleCollider> CreateCapsuleCollider(const CapsuleColliderDesc &desc) override;
		virtual Ref<PhysicsPlaneCollider> CreatePlaneCollider(const PlaneColliderDesc &desc) override;
		virtual Ref<PhysicsMeshCollider> CreateMeshCollider(const MeshColliderDesc &desc) override;
		virtual Ref<PhysicsHeightFieldCollider> CreateHeightFieldCollider(const HeightFieldColliderDesc &desc) override;

		virtual Ref<PhysicsCharacterController> CreateCharacterController(const CharacterControllerDesc &desc, uint64_t userData) override;

		virtual void DestroyDynamicBody(Ref<PhysicsDynamicActor> body) override;
		virtual void DestroyStaticBody(Ref<PhysicsStaticActor> body) override;
		virtual void DestroyCollider(Ref<PhysicsCollider> collider) override;
		virtual void DestroyCharacterController(Ref<PhysicsCharacterController> character) override;

		virtual bool Raycast(const Ray &ray, RaycastHit &outHit, float maxDistance = 1000.0f, uint32_t layerMask = 0xFFFFFFFF) override;

		virtual std::vector<CollisionEvent> DrainCollisionEvents() override;
		virtual std::vector<BodyActivationEvent> DrainActivationEvents() override;

		JPH::PhysicsSystem *GetPhysicsSystem() { return &m_PhysicsSystem; }
		JPH::BodyInterface *GetBodyInterface() const;

	private:
		Physics3DSettings m_Settings;

		JPH::BodyInterface *m_BodyInterface = nullptr;
		JPH::PhysicsSystem m_PhysicsSystem;

		Scope<JPH::TempAllocator> m_TempAllocator;
		Scope<JPH::JobSystem> m_JobSystem;
		Scope<JoltBodyActivationListener> m_BodyActivationListener;
		Scope<JoltContactListener> m_ContactListener;

		JoltBroadPhaseLayerInterfaceImpl m_BroadPhaseLayer;
		JoltObjectVsBroadPhaseLayerFilterImpl m_ObjectVsBroadPhaseLayerFilter;
		JoltObjectLayerPairFilterImpl m_ObjectLayerPairFilter;

		std::vector<Ref<PhysicsDynamicActor>> m_DynamicBodies;
		std::vector<Ref<PhysicsStaticActor>> m_StaticBodies;
		std::vector<Ref<PhysicsCharacterController>> m_CharacterControllers;
	};
}

#endif
