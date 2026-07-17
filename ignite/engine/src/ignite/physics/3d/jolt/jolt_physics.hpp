// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_JOLT_PHYSICS_HPP
#define IGN_JOLT_PHYSICS_HPP

#include "ignite/physics/3d/iphysics_3d.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#include <mutex>
#include <array>
#include <vector>

namespace ignite
{
    // -------------------------------------------------------------------------
    // Collision event types (mirrors Unity's OnCollisionEnter/Stay/Exit)
    // -------------------------------------------------------------------------
    enum class JoltCollisionEventType : uint8_t
    {
        Enter = 0,  // OnContactAdded
        Stay  = 1,  // OnContactPersisted
        Exit  = 2,  // OnContactRemoved
    };

    struct JoltCollisionEvent
    {
        JoltCollisionEventType type;
        JPH::BodyID bodyA;
        JPH::BodyID bodyB;
      
        // Contact point in world space (only valid for Enter/Stay)
        JPH::Vec3 contactPoint { 0.f, 0.f, 0.f };
        JPH::Vec3 contactNormal { 0.f, 1.f, 0.f };
    };

    enum class JoltActivationEventType : uint8_t
    {
        Activated = 0,
        Deactivated = 1,
    };

    struct JoltBodyActivationEvent
    {
		JoltActivationEventType type;
        JPH::BodyID bodyId;
    };

    // -------------------------------------------------------------------------
    // Result of a physics raycast (Jolt narrow-phase)
    // -------------------------------------------------------------------------
    struct JoltRaycastHit
    {
        bool hit = false;
        JPH::BodyID bodyId = {};
        float fraction = 1.0f;   // [0..1] along the ray
        glm::vec3 hitPoint = {};
        glm::vec3 hitNormal = {};
    };

    static JPH::Vec3 GlmToJoltVec3(const glm::vec3 &v)
    {
        return { v.x, v.y, v.z };
    }

    static glm::vec3 JoltToGlmVec3(const JPH::Vec3 &v)
    {
        return { v.GetX(), v.GetY(), v.GetZ() };
    }

    static JPH::Quat GlmToJoltQuat(const glm::quat &q)
    {
        return { q.x, q.y, q.z, q.w };
    }

    static glm::quat JoltToGlmQuat(const JPH::Quat &q)
    {
        return { q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
    }

    static void TraceImpl(const char *inFMT, ...)
    {
        // Format the message
        va_list list = { 0 };
        va_start(list, inFMT);
        std::array<char, 1024> buffer;
        vsnprintf(buffer.data(), buffer.size(), inFMT, list);
        va_end(list);

        // Print to the TTY
        LOG_INFO("{}", buffer.data());
    }

#ifdef JPH_ENABLE_ASSERTS

    // Callback for asserts, connect this to your own assert handler if you have one
    static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint32_t inLine)
    {
        // Print to the TTY
        LOG_ERROR("{}: {}: ({}) {}", inFile, inLine, inExpression, inMessage ? inMessage : "");

        // Breakpoint
        return true;
    };

#endif // JPH_ENABLE_ASSERTS

    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
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
        virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
        {
            switch (inObject1)
            {
                case Layers::NON_MOVING: return inObject2 == Layers::MOVING; // Non moving only collides with moving
                case Layers::MOVING: return true; // Moving collides with everything
                default:
                JPH_ASSERT(false);
                return false;
            }
        }
    };

    class JoltBroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        JoltBroadPhaseLayerInterfaceImpl()
        {
            m_ObjectToBP[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            m_ObjectToBP[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        virtual uint32_t GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            LOG_ASSERT(inLayer < Layers::NUM_LAYERS, "[Jolt] Invalid Layer");
            return m_ObjectToBP[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
            {
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING):	return "NON_MOVING";
                case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):		return "MOVING";
                default: JPH_ASSERT(false); return "INVALID";
            }
        }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED


    private:
        JPH::BroadPhaseLayer m_ObjectToBP[Layers::NUM_LAYERS];
    };

    class JoltObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
                case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING: return true;
                default:
                LOG_ASSERT(false, "Invalid layer!");
                return false;
            }
        }
    };

    // Jolt Contact Listener for collision events
    class JoltContactListener : public JPH::ContactListener
    {
    public:
        virtual JPH::ValidateResult OnContactValidate(const JPH::Body &bodyA, const JPH::Body &bodyB, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) override
        {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        virtual void OnContactAdded(const JPH::Body &bodyA, const JPH::Body &bodyB, const JPH::ContactManifold &manifold, JPH::ContactSettings &ioSettings) override
        {
            JoltCollisionEvent ev;
            ev.type = JoltCollisionEventType::Enter;
            ev.bodyA = bodyA.GetID();
            ev.bodyB = bodyB.GetID();
            ev.contactPoint = manifold.GetWorldSpaceContactPointOn1(0);
            ev.contactNormal = manifold.mWorldSpaceNormal;
            std::lock_guard<std::mutex> lock(m_EventMutex);
            m_PendingEvents.push_back(ev);
        }

        virtual void OnContactPersisted(const JPH::Body &bodyA, const JPH::Body &bodyB, const JPH::ContactManifold &manifold, JPH::ContactSettings &ioSettings) override
        {
            JoltCollisionEvent ev;
            ev.type = JoltCollisionEventType::Stay;
            ev.bodyA = bodyA.GetID();
            ev.bodyB = bodyB.GetID();
            ev.contactPoint = manifold.GetWorldSpaceContactPointOn1(0);
            ev.contactNormal = manifold.mWorldSpaceNormal;
            std::lock_guard<std::mutex> lock(m_EventMutex);
            m_PendingEvents.push_back(ev);
        }

        virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
        {
            JoltCollisionEvent ev;
            ev.type = JoltCollisionEventType::Exit;
            ev.bodyA = inSubShapePair.GetBody1ID();
            ev.bodyB = inSubShapePair.GetBody2ID();
            std::lock_guard<std::mutex> lock(m_EventMutex);
            m_PendingEvents.push_back(ev);
        }

        // Drain all accumulated events (called once per frame by the scene)
        std::vector<JoltCollisionEvent> DrainEvents()
        {
            std::lock_guard<std::mutex> lock(m_EventMutex);
            std::vector<JoltCollisionEvent> result;
            result.swap(m_PendingEvents);
            return result;
        }

    private:
        std::mutex m_EventMutex;
        std::vector<JoltCollisionEvent> m_PendingEvents;
    };

    // Jolt Body Activation Listener
    class JoltBodyActivationListener : public JPH::BodyActivationListener
    {
    public:
        virtual void OnBodyActivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData) override
        {
			JoltBodyActivationEvent ev;
			std::lock_guard<std::mutex> lock(m_EventMutex);
			ev.type = JoltActivationEventType::Activated;
			ev.bodyId = inBodyID;
			m_PendingEvents.push_back(ev);
        }

        virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData) override
        {
			JoltBodyActivationEvent ev;
            std::lock_guard<std::mutex> lock(m_EventMutex);
            ev.type = JoltActivationEventType::Deactivated;
			ev.bodyId = inBodyID;
			m_PendingEvents.push_back(ev);
        }

		// Drain all accumulated events (called once per frame by the scene)
		std::vector<JoltBodyActivationEvent> DrainEvents()
		{
			std::lock_guard<std::mutex> lock(m_EventMutex);
			std::vector<JoltBodyActivationEvent> result;
			result.swap(m_PendingEvents);
			return result;
		}

    private:
		std::mutex m_EventMutex;
		std::vector<JoltBodyActivationEvent> m_PendingEvents;
    };

    class IGN_API JoltPhysics : public IPhysics3D
    {
    public:
        virtual void Init() override;
        virtual void Shutdown() override;

        static JoltPhysics *GetInstance();

        Scope<JPH::TempAllocator> tempAllocator;
        Scope<JPH::JobSystem> jobSystem;
        Scope<JPH::BodyActivationListener> bodyActivationListener;
        Scope<JPH::ContactListener> contactListener;

        JoltBroadPhaseLayerInterfaceImpl broadPhaseLayer;
        JoltObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
        JoltObjectLayerPairFilterImpl objectLayerPairFilter;
    };

    class Scene;

    class IGN_API JoltScene
    {
    public:
        JoltScene() = default;
        ~JoltScene();

        void SimulationStart(Scene *scene);
        void SimulationStop();

        void Simulate(float deltaTime);

        void InstantiateEntity(Entity entity);
        void DestroyEntity(Entity entity);

        JPH::BodyCreationSettings CreateBody(JPH::ShapeRefC shape, RigidbodyComponent &rb, const glm::vec3 &position, const glm::quat &rotation);

        void CreatePlaneCollider(Entity entity);
        void CreateBoxCollider(Entity entity);
        void CreateCapsuleCollider(Entity entity);
        void CreateSphereCollider(Entity entity);
        void CreateMeshCollider(Entity entity);

        // Narrow-phase ray cast (returns first hit)
        JoltRaycastHit Raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance = 1000.0f);

        // Drain collision events accumulated this frame
        std::vector<JoltCollisionEvent> DrainCollisionEvents();

		std::vector<JoltBodyActivationEvent> DrainActivationEvents();

        JPH::uint64 GetUserData(const JPH::BodyID &bodyId);

        void AddForce(const JPH::Body &body, const glm::vec3 &force);
        void AddTorque(const JPH::Body &body, const glm::vec3 &torque);
        void AddForceAndTorque(const JPH::Body &body, const glm::vec3 &force, const glm::vec3 &torque);
        void AddAngularImpulse(const JPH::Body &body, const glm::vec3 &impulse);
        void ActivateBody(const JPH::Body &body);
        void DeactivateBody(const JPH::Body &body);
        void DestroyBody(const JPH::Body &body);
        bool IsActive(const JPH::Body &body);
        void MoveKinematic(const JPH::Body &body, const glm::vec3 &targetPosition, const glm::vec3 &targetRotation, float deltaTime);
        void AddImpulse(const JPH::Body &body, const glm::vec3 &impulse);
        void AddLinearVelocity(const JPH::Body &body, const glm::vec3 &velocity);
        void SetPosition(const JPH::Body &body, const glm::vec3 &position, bool activate);
        void SetEulerAngleRotation(const JPH::Body &body, const glm::vec3 &rotation, bool activate);
        void SetRotation(const JPH::Body &body, const glm::quat &rotation, bool activate);
        void SetLinearVelocity(const JPH::Body &body, const glm::vec3 &vel);
        void SetFriction(const JPH::Body &body, float value);
        void SetRestitution(const JPH::Body &body, float value);
        void SetGravityFactor(const JPH::Body &body, float value);
        void SetMaxLinearVelocity(JPH::Body &body, float max);
        void SetMaxAngularVelocity(JPH::Body &body, float max);
		void SetMassProperties(JPH::Body &body, const JPH::MassProperties &props);
        float GetRestitution(const JPH::Body &body);
        float GetFriction(const JPH::Body &body);
        float GetGravityFactor(const JPH::Body &body);
        glm::vec3 GetPosition(const JPH::Body &body);
        glm::vec3 GetEulerAngles(const JPH::Body &body);
        glm::quat GetRotation(const JPH::Body &body);
        glm::vec3 GetCenterOfMassPosition(const JPH::Body &body);
        glm::vec3 GetLinearVelocity(const JPH::Body &body);

        JPH::BodyInterface *GetBodyInterface() const;
    private:
        Scene *m_Scene;
        JPH::BodyInterface *m_BodyInterface = nullptr;
        JPH::PhysicsSystem m_PhysicsSystem;

    };
}

#endif
