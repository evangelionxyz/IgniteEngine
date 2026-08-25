// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "jolt_physics.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

namespace ignite::physics
{
	static constexpr int cMaxPhysicsJobs = 2048;
	static constexpr unsigned int cNumBodies = 20480;
	static constexpr unsigned int cNumBodyMutexes = 0;
	static constexpr unsigned int cMaxBodyPairs = 64000;
	static constexpr unsigned int cMaxContactConstraints = 20480;
	static constexpr unsigned int cAllocatorSize = 32 * 1024 * 1024;

	using namespace JPH::literals;

	JoltPhysics::JoltPhysics()
	{
		JPH::RegisterDefaultAllocator();

		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		m_TempAllocator = CreateScope<JPH::TempAllocatorImpl>(cAllocatorSize);
		m_JobSystem = CreateScope<JPH::JobSystemThreadPool>(cMaxPhysicsJobs, 8, std::thread::hardware_concurrency() - 1);

		m_ContactListener = CreateScope<JoltContactListener>();
		m_BodyActivationListener = CreateScope<JoltBodyActivationListener>();

		m_ObjectLayerPairFilter.settings = &m_Settings;
		m_PhysicsSystem.Init(cNumBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
			m_BroadPhaseLayer, m_ObjectVsBroadPhaseLayerFilter, m_ObjectLayerPairFilter);

		m_PhysicsSystem.SetBodyActivationListener(m_BodyActivationListener.get());
		m_PhysicsSystem.SetContactListener(m_ContactListener.get());
		m_BodyInterface = &m_PhysicsSystem.GetBodyInterface();

		IGN_PHYSICS_WARN("[Jolt Physics] Initialized");
	}

	JoltPhysics::~JoltPhysics()
	{
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;

		IGN_PHYSICS_WARN("[Jolt Physics] Shutdown");
	}

	void JoltPhysics::SimulationStart(const Physics3DSettings &settings)
	{
		IGN_PROFILE_FUNCTION();
		m_Settings = settings;
		m_ObjectLayerPairFilter.settings = &m_Settings;
		m_PhysicsSystem.SetGravity({ settings.gravity.x, settings.gravity.y, settings.gravity.z });
		m_PhysicsSystem.OptimizeBroadPhase();
	}

	void JoltPhysics::SimulationStop()
	{
		IGN_PROFILE_FUNCTION();
		for (auto &b : m_DynamicBodies)
			if (b) b->DestroyBody();
		for (auto &b : m_StaticBodies)
			if (b) b->DestroyBody();
		
		m_DynamicBodies.clear();
		m_StaticBodies.clear();
		m_CharacterControllers.clear();
	}

	void JoltPhysics::Simulate(float deltaTime)
	{
		IGN_PROFILE_FUNCTION();
		constexpr int cCollisionSteps = 1;
		m_PhysicsSystem.Update(deltaTime, cCollisionSteps, m_TempAllocator.get(), m_JobSystem.get());
	}

	static JPH::ShapeRefC GetJoltShapeFromCollider(const Ref<PhysicsCollider> &collider)
	{
		if (!collider) return nullptr;
		switch (collider->GetColliderType())
		{
		case ColliderType::Box:
			return std::static_pointer_cast<JoltBoxCollider>(collider)->GetJoltShape();
		case ColliderType::Sphere:
			return std::static_pointer_cast<JoltSphereCollider>(collider)->GetJoltShape();
		case ColliderType::Capsule:
			return std::static_pointer_cast<JoltCapsuleCollider>(collider)->GetJoltShape();
		case ColliderType::Plane:
			return std::static_pointer_cast<JoltPlaneCollider>(collider)->GetJoltShape();
		case ColliderType::Mesh:
			return std::static_pointer_cast<JoltMeshCollider>(collider)->GetJoltShape();
		case ColliderType::HeightField:
			return std::static_pointer_cast<JoltHeightFieldCollider>(collider)->GetJoltShape();
		}
		return nullptr;
	}

	Ref<PhysicsDynamicActor> JoltPhysics::CreateDynamicBody(const RigidBodyDesc &desc, const PhysicsTransformData &transform, uint64_t userData, const Ref<PhysicsCollider> &collider)
	{
		JPH::ShapeRefC shape = GetJoltShapeFromCollider(collider);
		if (!shape)
		{
			shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
		}

		JPH::BodyCreationSettings bodySettings(
			shape,
			JPH::RVec3(transform.position.x, transform.position.y, transform.position.z),
			JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
			desc.bodyType == BodyType::Kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic,
			static_cast<JPH::ObjectLayer>(desc.layer)
		);

		bodySettings.mUserData = userData;
		bodySettings.mFriction = desc.friction;
		bodySettings.mRestitution = desc.restitution;
		bodySettings.mLinearDamping = desc.linearDamping;
		bodySettings.mAngularDamping = desc.angularDamping;
		bodySettings.mGravityFactor = desc.useGravity ? desc.gravityFactor : 0.0f;
		bodySettings.mIsSensor = desc.isSensor;
		bodySettings.mAllowSleeping = desc.allowSleeping;

		bodySettings.mAllowedDOFs = JPH::EAllowedDOFs::None;
		if (desc.rotateX) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::RotationX;
		if (desc.rotateY) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::RotationY;
		if (desc.rotateZ) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::RotationZ;
		if (desc.moveX) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::TranslationX;
		if (desc.moveY) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::TranslationY;
		if (desc.moveZ) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::TranslationZ;

		JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
		if (!body) return nullptr;

		m_BodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);
		auto bodyRef = CreateRef<JoltDynamicPhysicsBody>(body->GetID().GetIndexAndSequenceNumber(), m_BodyInterface);
		m_DynamicBodies.push_back(bodyRef);
		return bodyRef;
	}

	Ref<PhysicsStaticActor> JoltPhysics::CreateStaticBody(const RigidBodyDesc &desc, const PhysicsTransformData &transform, uint64_t userData, const Ref<PhysicsCollider> &collider)
	{
		JPH::ShapeRefC shape = GetJoltShapeFromCollider(collider);
		if (!shape)
		{
			shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
		}

		JPH::BodyCreationSettings bodySettings(
			shape,
			JPH::RVec3(transform.position.x, transform.position.y, transform.position.z),
			JPH::Quat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w),
			JPH::EMotionType::Static,
			static_cast<JPH::ObjectLayer>(desc.layer)
		);

		bodySettings.mUserData = userData;
		bodySettings.mFriction = desc.friction;
		bodySettings.mRestitution = desc.restitution;

		JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
		if (!body)
			return nullptr;

		m_BodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);
		auto bodyRef = CreateRef<JoltStaticPhysicsBody>(body->GetID().GetIndexAndSequenceNumber(), m_BodyInterface);
		m_StaticBodies.push_back(bodyRef);
		return bodyRef;
	}

	Ref<PhysicsBoxCollider> JoltPhysics::CreateBoxCollider(const BoxColliderDesc &desc)
	{
		return CreateRef<JoltBoxCollider>(desc);
	}

	Ref<PhysicsSphereCollider> JoltPhysics::CreateSphereCollider(const SphereColliderDesc &desc)
	{
		return CreateRef<JoltSphereCollider>(desc);
	}

	Ref<PhysicsCapsuleCollider> JoltPhysics::CreateCapsuleCollider(const CapsuleColliderDesc &desc)
	{
		return CreateRef<JoltCapsuleCollider>(desc);
	}

	Ref<PhysicsPlaneCollider> JoltPhysics::CreatePlaneCollider(const PlaneColliderDesc &desc)
	{
		return CreateRef<JoltPlaneCollider>(desc);
	}

	Ref<PhysicsMeshCollider> JoltPhysics::CreateMeshCollider(const MeshColliderDesc &desc)
	{
		return CreateRef<JoltMeshCollider>(desc);
	}

	Ref<PhysicsHeightFieldCollider> JoltPhysics::CreateHeightFieldCollider(const HeightFieldColliderDesc &desc)
	{
		return CreateRef<JoltHeightFieldCollider>(desc);
	}

	Ref<PhysicsCharacterController> JoltPhysics::CreateCharacterController(const CharacterControllerDesc &desc, uint64_t userData)
	{
		auto ccRef = CreateRef<JoltCharacterController>(desc, userData, &m_PhysicsSystem, m_TempAllocator.get());
		m_CharacterControllers.push_back(ccRef);
		return ccRef;
	}

	void JoltPhysics::DestroyDynamicBody(Ref<PhysicsDynamicActor> body)
	{
		if (body)
		{
			body->DestroyBody();
			std::erase(m_DynamicBodies, body);
		}
	}

	void JoltPhysics::DestroyStaticBody(Ref<PhysicsStaticActor> body)
	{
		if (body)
		{
			body->DestroyBody();
			std::erase(m_StaticBodies, body);
		}
	}

	void JoltPhysics::DestroyCollider(Ref<PhysicsCollider> collider)
	{
	}

	void JoltPhysics::DestroyCharacterController(Ref<PhysicsCharacterController> character)
	{
		if (character)
		{
			std::erase(m_CharacterControllers, character);
		}
	}

	class JoltRaycastLayerFilter : public JPH::ObjectLayerFilter
	{
	public:
		uint32_t mask = 0xFFFFFFFF;
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const override
		{
			return (mask & (1u << static_cast<uint32_t>(inLayer))) != 0;
		}
	};

	bool JoltPhysics::Raycast(const Ray &ray, RaycastHit &outHit, float maxDistance, uint32_t layerMask)
	{
		JPH::RRayCast rayCast(
			JPH::RVec3(ray.origin.x, ray.origin.y, ray.origin.z),
			JPH::Vec3(ray.direction.x * maxDistance, ray.direction.y * maxDistance, ray.direction.z * maxDistance)
		);

		JPH::RayCastResult hit;
		JoltRaycastLayerFilter layerFilter;
		layerFilter.mask = layerMask;

		if (m_PhysicsSystem.GetNarrowPhaseQuery().CastRay(rayCast, hit, JPH::SpecifiedBroadPhaseLayerFilter(BroadPhaseLayers::MOVING), layerFilter))
		{
			outHit.fraction = hit.mFraction;
			JPH::Vec3 hitPos = rayCast.GetPointOnRay(hit.mFraction);
			outHit.hitPoint = { hitPos.GetX(), hitPos.GetY(), hitPos.GetZ() };
			outHit.userData = m_BodyInterface->GetUserData(hit.mBodyID);
			return true;
		}

		return false;
	}

	std::vector<CollisionEvent> JoltPhysics::DrainCollisionEvents()
	{
		return m_ContactListener ? m_ContactListener->DrainEvents() : std::vector<CollisionEvent>{};
	}

	std::vector<BodyActivationEvent> JoltPhysics::DrainActivationEvents()
	{
		return m_BodyActivationListener ? m_BodyActivationListener->DrainEvents() : std::vector<BodyActivationEvent>{};
	}

	JPH::BodyInterface *JoltPhysics::GetBodyInterface() const
	{
		return m_BodyInterface;
	}
}
