// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "jolt_physics.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/scene.hpp"

#include <Jolt/Core/Factory.h>

namespace ignite
{
    static constexpr int cMaxPhysicsJobs = 2048;
    static constexpr unsigned int cNumBodies = 20480;
    static constexpr unsigned int cNumBodyMutexes = 0;
    static constexpr unsigned int cMaxBodyPairs = 64000;
    static constexpr unsigned int cMaxContactConstraints = 20480;
	static constexpr unsigned int cAllocatorSize = 32 * 1024 * 1024;

    using namespace JPH::literals;

    static JoltPhysics *s_JoltInstance = nullptr;

    void JoltPhysics::Init()
    {
        s_JoltInstance = new JoltPhysics();

        JPH::RegisterDefaultAllocator();

        JPH::Trace = TraceImpl;
        //JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl);

        JPH::Factory::sInstance = new JPH::Factory();
        
        JPH::RegisterTypes();
        
        s_JoltInstance->tempAllocator = CreateScope<JPH::TempAllocatorImpl>(cAllocatorSize);
        s_JoltInstance->jobSystem = CreateScope<JPH::JobSystemThreadPool>(cMaxPhysicsJobs, 8,
            std::thread::hardware_concurrency() - 1);

        // Create collision listeners
        s_JoltInstance->contactListener = CreateScope<JoltContactListener>();
        s_JoltInstance->bodyActivationListener = CreateScope<JoltBodyActivationListener>();

        LOG_WARN("[Jolt Physics] Initalized");
    }

    void JoltPhysics::Shutdown()
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        if (s_JoltInstance)
        {
           delete s_JoltInstance;
		   s_JoltInstance = nullptr;
        }

        LOG_WARN("[Jolt Physics] Shutdown");
    }

    JoltPhysics *JoltPhysics::GetInstance()
    {
        return s_JoltInstance;
    }

    JoltScene::~JoltScene()
    {
        SimulationStop();
    }

    void JoltScene::SimulationStart(Scene *scene)
    {
        IGN_PROFILE_FUNCTION();
        m_Scene = scene;

        m_PhysicsSystem.Init(cNumBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            s_JoltInstance->broadPhaseLayer,
            s_JoltInstance->objectVsBroadPhaseLayerFilter,
            s_JoltInstance->objectLayerPairFilter);

        m_PhysicsSystem.SetBodyActivationListener(s_JoltInstance->bodyActivationListener.get());
        m_PhysicsSystem.SetContactListener(s_JoltInstance->contactListener.get());
        m_PhysicsSystem.SetGravity(GlmToJoltVec3(m_Scene->physicsGravity));
        m_PhysicsSystem.OptimizeBroadPhase();

        m_BodyInterface = &m_PhysicsSystem.GetBodyInterface();

        for (entt::entity e : m_Scene->registry->view<RigidbodyComponent>())
        {
            InstantiateEntity(Entity { e, m_Scene });
        }
    }

    void JoltScene::SimulationStop()
    {
        IGN_PROFILE_FUNCTION();

        if (!m_BodyInterface || !m_Scene)
        {
            return;
        }

        for (entt::entity e : m_Scene->registry->view<RigidbodyComponent>())
        {
            DestroyEntity(Entity { e, m_Scene });
        }

        m_BodyInterface = nullptr;
        m_Scene = nullptr;
    }

    void JoltScene::Simulate(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        {
            IGN_PROFILE_SCOPE("JoltScene::SyncEntitiesFromPhysics");
            for (const auto id : m_Scene->registry->view<RigidbodyComponent>())
            {
                Entity entity = { id, m_Scene };
                const RigidbodyComponent &rb = entity.GetComponent<RigidbodyComponent>();
                TransformComponent &tr = entity.GetComponent<TransformComponent>();
                IDComponent &idc = entity.GetComponent<IDComponent>();

                if (!rb.body)
                    continue;

                if (entity.HasComponent<BoxColliderComponent>())
                {
                    auto &col = entity.GetComponent<BoxColliderComponent>();
                    if (col.dirty)
                    {
                        glm::vec3 halfExtents = col.scale * tr.world.scale;
                        if (halfExtents.x > 0.0f && halfExtents.y > 0.0f && halfExtents.z > 0.0f)
                        {
                            JPH::BoxShapeSettings shapeSettings(GlmToJoltVec3(halfExtents));
                            JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
                            if (shapeResult.IsValid() && !shapeResult.HasError())
                            {
                                JPH::ShapeRefC shape = shapeResult.Get();
                                JPH::RotatedTranslatedShapeSettings offsetSettings(GlmToJoltVec3(col.center * tr.world.scale), JPH::Quat::sIdentity(), shape.GetPtr());
                                JPH::ShapeSettings::ShapeResult offsetResult = offsetSettings.Create();
                                if (offsetResult.IsValid() && !offsetResult.HasError())
                                {
                                    shape = offsetResult.Get();
                                    m_BodyInterface->SetShape(rb.body->GetID(), shape.GetPtr(), true, JPH::EActivation::Activate);
                                    m_BodyInterface->SetFriction(rb.body->GetID(), col.friction);
                                    m_BodyInterface->SetRestitution(rb.body->GetID(), col.restitution);
                                    col.shape = (void *)shape.GetPtr();
                                }
                            }
                        }
                        col.dirty = false;
                    }
                }

                if (entity.HasComponent<SphereColliderComponent>())
                {
                    auto &col = entity.GetComponent<SphereColliderComponent>();
                    if (col.dirty)
                    {
                        float maxAxis = glm::compMax(tr.world.scale);
                        float effectiveRadius = col.radius * maxAxis;
                        if (effectiveRadius > 0.0f)
                        {
                            JPH::SphereShapeSettings shapeSettings(effectiveRadius);
                            JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
                            if (shapeResult.IsValid() && !shapeResult.HasError())
                            {
                                JPH::ShapeRefC shape = shapeResult.Get();
                                JPH::RotatedTranslatedShapeSettings offsetSettings(GlmToJoltVec3(col.center * maxAxis), JPH::Quat::sIdentity(), shape.GetPtr());
                                JPH::ShapeSettings::ShapeResult offsetResult = offsetSettings.Create();
                                if (offsetResult.IsValid() && !offsetResult.HasError())
                                {
                                    shape = offsetResult.Get();
                                    m_BodyInterface->SetShape(rb.body->GetID(), shape.GetPtr(), true, JPH::EActivation::Activate);
                                    m_BodyInterface->SetFriction(rb.body->GetID(), col.friction);
                                    m_BodyInterface->SetRestitution(rb.body->GetID(), col.restitution);
                                    col.shape = (void *)shape.GetPtr();
                                }
                            }
                        }
                        col.dirty = false;
                    }
                }

                if (entity.HasComponent<CapsuleColliderComponent>())
                {
                    auto &col = entity.GetComponent<CapsuleColliderComponent>();
                    if (col.dirty)
                    {
                        float maxScale = glm::compMax(tr.world.scale);
                        float halfHeight = col.height * 0.5f * maxScale;
                        float radius = col.radius * maxScale;
                        if (halfHeight > 0.0f && radius > 0.0f)
                        {
                            JPH::CapsuleShapeSettings capsuleShapeSettings(halfHeight, radius);
                            JPH::ShapeSettings::ShapeResult capsuleShapeResult = capsuleShapeSettings.Create();
                            if (capsuleShapeResult.IsValid() && !capsuleShapeResult.HasError())
                            {
                                JPH::ShapeRefC capsuleShape = capsuleShapeResult.Get();
                                const glm::quat horizontalRotation = glm::angleAxis(1.57079632679f, glm::vec3(1.0f, 0.0f, 0.0f));
                                JPH::RotatedTranslatedShapeSettings shapeSettings(GlmToJoltVec3(col.center * maxScale), GlmToJoltQuat(horizontalRotation), capsuleShape.GetPtr());
                                JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
                                if (shapeResult.IsValid() && !shapeResult.HasError())
                                {
                                    JPH::ShapeRefC shape = shapeResult.Get();
                                    m_BodyInterface->SetShape(rb.body->GetID(), shape.GetPtr(), true, JPH::EActivation::Activate);
                                    m_BodyInterface->SetFriction(rb.body->GetID(), col.friction);
                                    m_BodyInterface->SetRestitution(rb.body->GetID(), col.restitution);
                                    col.shape = (void *)shape.GetPtr();
                                }
                            }
                        }
                        col.dirty = false;
                    }
                }

                if (entity.HasComponent<MeshColliderComponent>())
                {
                    auto &col = entity.GetComponent<MeshColliderComponent>();
                    if (col.dirty)
                    {
                        if (!col.vertices.empty())
                        {
                            JPH::ShapeRefC shape;
                            if (col.convex)
                            {
                                JPH::Array<JPH::Vec3> vertices;
                                vertices.reserve(col.vertices.size());
                                for (const auto& vertex : col.vertices)
                                {
                                    vertices.push_back(GlmToJoltVec3(vertex));
                                }
                                JPH::ConvexHullShapeSettings shapeSettings(vertices);
                                JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
                                if (shapeResult.IsValid() && !shapeResult.HasError())
                                {
                                    shape = shapeResult.Get();
                                }
                            }
                            else
                            {
                                JPH::TriangleList triangles;
                                if (col.indices.empty())
                                {
                                    if (col.vertices.size() % 3 == 0)
                                    {
                                        for (size_t i = 0; i < col.vertices.size(); i += 3)
                                        {
                                            JPH::Triangle triangle(
                                                GlmToJoltVec3(col.vertices[i]),
                                                GlmToJoltVec3(col.vertices[i + 1]),
                                                GlmToJoltVec3(col.vertices[i + 2])
                                            );
                                            triangles.push_back(triangle);
                                        }
                                    }
                                }
                                else
                                {
                                    for (size_t i = 0; i < col.indices.size(); i += 3)
                                    {
                                        if (i + 2 < col.indices.size() &&
                                            col.indices[i] < col.vertices.size() &&
                                            col.indices[i + 1] < col.vertices.size() &&
                                            col.indices[i + 2] < col.vertices.size())
                                        {
                                            JPH::Triangle triangle(
                                                GlmToJoltVec3(col.vertices[col.indices[i]]),
                                                GlmToJoltVec3(col.vertices[col.indices[i + 1]]),
                                                GlmToJoltVec3(col.vertices[col.indices[i + 2]])
                                            );
                                            triangles.push_back(triangle);
                                        }
                                    }
                                }

                                if (!triangles.empty())
                                {
                                    JPH::MeshShapeSettings shapeSettings(triangles);
                                    JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
                                    if (shapeResult.IsValid() && !shapeResult.HasError())
                                    {
                                        shape = shapeResult.Get();
                                    }
                                }
                            }

                            if (shape)
                            {
                                m_BodyInterface->SetShape(rb.body->GetID(), shape.GetPtr(), true, JPH::EActivation::Activate);
                                m_BodyInterface->SetFriction(rb.body->GetID(), col.friction);
                                m_BodyInterface->SetRestitution(rb.body->GetID(), col.restitution);
                                col.shape = (void *)shape.GetPtr();
                            }
                        }
                        col.dirty = false;
                    }
                }

                if (tr.dirtyPhysics)
                {
                    SetPosition(*rb.body, tr.world.translation, true);
                    SetRotation(*rb.body, tr.world.rotation, true);
                    tr.dirtyPhysics = false;
                }
                else
                {
                    // we don't care about the parent
                    tr.local.translation = JoltToGlmVec3(rb.body->GetPosition());
                    tr.local.rotation = JoltToGlmQuat(rb.body->GetRotation());
                    tr.world.translation = tr.local.translation;
                    tr.world.rotation = tr.local.rotation;
                }
            }
        }

        {
            IGN_PROFILE_SCOPE("JoltScene::PhysicsUpdate");
            m_PhysicsSystem.Update(deltaTime, 1, s_JoltInstance->tempAllocator.get(), s_JoltInstance->jobSystem.get());
        }
    }

    void JoltScene::InstantiateEntity(Entity entity)
    {
        // Guard against race/timing: body interface must be initialized before creating bodies
        if (!m_BodyInterface)
        {
            LOG_WARN("[Jolt Physics] InstantiateEntity called but BodyInterface is not initialized. Skipping entity {}", (uint64_t)entity.GetUUID());
            return;
        }

        if (!entity.HasComponent<RigidbodyComponent>())
            return;

        auto &rb = entity.GetComponent<RigidbodyComponent>();

        // Prevent duplicate runtime state: if a body already exists for this component, skip creating another
        if (rb.body)
        {
            LOG_WARN("[Jolt Physics] Entity {} already has a Jolt body; skipping duplicate instantiation", (uint64_t)entity.GetUUID());
            return;
        }

        if (entity.HasComponent<BoxColliderComponent>())
        {
            CreateBoxCollider(entity);
        }

        if (entity.HasComponent<SphereColliderComponent>())
        {
            CreateSphereCollider(entity);
        }

        if (entity.HasComponent<CapsuleColliderComponent>())
        {
            CreateCapsuleCollider(entity);
        }

        if (entity.HasComponent<MeshColliderComponent>())
        {
            CreateMeshCollider(entity);
        }
    }

    void JoltScene::DestroyEntity(Entity entity)
    {
        if (entity.HasComponent<RigidbodyComponent>())
        {
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (rb.body)
            {
                m_BodyInterface->RemoveBody(rb.body->GetID());
                m_BodyInterface->DestroyBody(rb.body->GetID());
                rb.body = nullptr;
            }
        }
    }

    JPH::BodyCreationSettings JoltScene::CreateBody(JPH::ShapeRefC shape, RigidbodyComponent &rb, const glm::vec3 &position, const glm::quat &rotation)
    {
        const auto motionType = static_cast<JPH::EMotionType>(rb.bodyType);
        
        JPH::BodyCreationSettings bodySettings(shape,
            GlmToJoltVec3(position),
            GlmToJoltQuat(rotation),
            motionType,
            motionType == JPH::EMotionType::Static ? Layers::NON_MOVING : Layers::MOVING);

        bodySettings.mMotionQuality = static_cast<JPH::EMotionQuality>(rb.motionQuality);

        bodySettings.mAllowedDOFs = JPH::EAllowedDOFs::None;
        if (rb.rotateX) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::RotationX;
        if (rb.rotateY) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::RotationY;
        if (rb.rotateZ) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::RotationZ;
        if (rb.moveX) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::TranslationX;
        if (rb.moveY) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::TranslationY;
        if (rb.moveZ) bodySettings.mAllowedDOFs |= JPH::EAllowedDOFs::TranslationZ;

        return bodySettings;
    }

    JPH::BodyInterface *JoltScene::GetBodyInterface() const
    {
        return m_BodyInterface;
    }

    void JoltScene::CreatePlaneCollider(Entity entity)
    {
        auto &tr = entity.GetComponent<TransformComponent>();
        auto &rb = entity.GetComponent<RigidbodyComponent>();
        auto &col = entity.GetComponent<PlaneColliderComponent>();

        glm::vec3 halfExtents = col.scale * tr.world.scale;
        JPH::Plane inPlane(JPH::Vec3Arg{0.0f, 1.0f, 0.0f}, 1.0f);
        JPH::PlaneShapeSettings planeShapeSettings(inPlane);
        JPH::ShapeSettings::ShapeResult shapeResult = planeShapeSettings.Create();
        if (!shapeResult.IsValid() || shapeResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings!", shapeResult.GetError());
            return;
        }

        JPH::ShapeRefC shape = shapeResult.Get();

        // Wrap with offset
        JPH::RotatedTranslatedShapeSettings offsetSettings(GlmToJoltVec3(col.center * tr.world.scale), JPH::Quat::sIdentity(), shape.GetPtr());
        JPH::ShapeSettings::ShapeResult offsetResult = offsetSettings.Create();
        if (!offsetResult.IsValid() || offsetResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", offsetResult.GetError());
            return;
        }
        shape = offsetResult.Get();

        JPH::BodyCreationSettings bodySettings = CreateBody(shape, rb, tr.world.translation, tr.world.rotation);

        JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
        if (body)
        {
            JPH::BodyID bodyId = body->GetID();
            m_BodyInterface->AddBody(bodyId, JPH::EActivation::Activate);
            m_BodyInterface->SetFriction(bodyId, col.friction);
            m_BodyInterface->SetRestitution(bodyId, col.restitution);
            body->SetUserData((uint64_t)entity.GetUUID());
            rb.body = body;
        }

        col.shape = (void *)shape.GetPtr();
    }

    void JoltScene::CreateBoxCollider(Entity entity)
    {
        auto &tr = entity.GetComponent<TransformComponent>();
        auto &rb = entity.GetComponent<RigidbodyComponent>();
        auto &col = entity.GetComponent<BoxColliderComponent>();

        // Validate transform and scales
        if (!std::isfinite(tr.world.scale.x) || !std::isfinite(tr.world.scale.y) || !std::isfinite(tr.world.scale.z))
        {
            LOG_WARN("[Jolt Physics] Invalid transform scale for entity {}: {},{},{} - skipping collider creation",
                (uint64_t)entity.GetUUID(), tr.world.scale.x, tr.world.scale.y, tr.world.scale.z);
            return;
        }

        glm::vec3 halfExtents = col.scale * tr.world.scale;

        // Ensure extents are positive and non-zero
        if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
        {
            LOG_WARN("[Jolt Physics] Box collider for entity {} has non-positive half extents {},{},{} - skipping",
                (uint64_t)entity.GetUUID(), halfExtents.x, halfExtents.y, halfExtents.z);
            return;
        }

        JPH::BoxShapeSettings shapeSettings(GlmToJoltVec3(halfExtents));

        JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
        if (!shapeResult.IsValid() || shapeResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", shapeResult.GetError());
            return;
        }

        JPH::ShapeRefC shape = shapeResult.Get();

        // Wrap with offset to match renderer visualizer (using entity transform origin as body origin)
        JPH::RotatedTranslatedShapeSettings offsetSettings(GlmToJoltVec3(col.center * tr.world.scale), JPH::Quat::sIdentity(), shape.GetPtr());
        JPH::ShapeSettings::ShapeResult offsetResult = offsetSettings.Create();
        if (!offsetResult.IsValid() || offsetResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", offsetResult.GetError());
            return;
        }
        shape = offsetResult.Get();

        JPH::BodyCreationSettings bodySettings = CreateBody(shape, rb, tr.world.translation, tr.world.rotation);

        // Extra safety: ensure body pointer is still null to avoid duplicates
        if (rb.body)
        {
            LOG_WARN("[Jolt Physics] Entity {} already has a body when creating box collider - removing existing body", (uint64_t)entity.GetUUID());
            if (m_BodyInterface)
            {
                m_BodyInterface->RemoveBody(rb.body->GetID());
                m_BodyInterface->DestroyBody(rb.body->GetID());
            }
            rb.body = nullptr;
        }

        JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
        if (body)
        {
            JPH::BodyID bodyId = body->GetID();
            m_BodyInterface->AddBody(bodyId, JPH::EActivation::Activate);
            m_BodyInterface->SetFriction(bodyId, col.friction);
            m_BodyInterface->SetRestitution(bodyId, col.restitution);
            body->SetUserData((uint64_t)entity.GetUUID());
            rb.body = body;
        }

        col.shape = (void *)shape.GetPtr();
    }

    void JoltScene::CreateCapsuleCollider(Entity entity)
    {
        auto &tr = entity.GetComponent<TransformComponent>();
        auto &rb = entity.GetComponent<RigidbodyComponent>();
        auto &col = entity.GetComponent<CapsuleColliderComponent>();

        // Validate scales
        if (!std::isfinite(tr.world.scale.x) || !std::isfinite(tr.world.scale.y) || !std::isfinite(tr.world.scale.z))
        {
            LOG_WARN("[Jolt Physics] Invalid transform scale for entity {} when creating capsule: {},{},{} - skipping",
                (uint64_t)entity.GetUUID(), tr.world.scale.x, tr.world.scale.y, tr.world.scale.z);
            return;
        }

        const float maxScale = glm::compMax(glm::abs(tr.world.scale));
		const float radius = col.radius * maxScale;

		// Jolt needs half of the cylinder height (excluding the hemispherical ends) for the capsule shape.
        // The total height of the capsule is col.height, so we subtract the radius from half of that to get the cylinder half-height.
        const float cylinderHalfHeight = glm::max(col.height * 0.5f - col.radius, 0.0f) * maxScale;

		JPH::CapsuleShapeSettings capsuleShapeSettings(cylinderHalfHeight, radius);
        if (!capsuleShapeSettings.IsValid())
        {
			LOG_WARN("[Jolt Physics] Capsule collider for entity {} has non-positive dimensions halfHeight={}, radius={} - skipping",
				(uint64_t)entity.GetUUID(), cylinderHalfHeight, radius);
			return;
        }

        JPH::ShapeSettings::ShapeResult shapeResult = capsuleShapeSettings.Create();
        if (!shapeResult.IsValid() || shapeResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", shapeResult.GetError());
            return;
        }

        JPH::ShapeRefC capsuleShape = shapeResult.Get();
        JPH::BodyCreationSettings bodySettings = CreateBody(capsuleShape, rb, tr.world.translation, tr.world.rotation);

        if (rb.body)
        {
            LOG_WARN("[Jolt Physics] Entity {} already has a body when creating capsule - removing existing body", (uint64_t)entity.GetUUID());
            if (m_BodyInterface)
            {
                m_BodyInterface->RemoveBody(rb.body->GetID());
                m_BodyInterface->DestroyBody(rb.body->GetID());
            }
            rb.body = nullptr;
        }

        JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
        if (body)
        {
            JPH::BodyID bodyId = body->GetID();
            m_BodyInterface->AddBody(bodyId, JPH::EActivation::Activate);
            m_BodyInterface->SetFriction(bodyId, col.friction);
            m_BodyInterface->SetRestitution(bodyId, col.restitution);
            body->SetUserData((uint64_t)entity.GetUUID());
            rb.body = body;
        }

        col.shape = (void *)capsuleShape.GetPtr();
    }

    void JoltScene::CreateSphereCollider(Entity entity)
    {
        auto &tr = entity.GetComponent<TransformComponent>();
        auto &rb = entity.GetComponent<RigidbodyComponent>();
        auto &col = entity.GetComponent<SphereColliderComponent>();

        // Validate scale
        if (!std::isfinite(tr.world.scale.x) || !std::isfinite(tr.world.scale.y) || !std::isfinite(tr.world.scale.z))
        {
            LOG_WARN("[Jolt Physics] Invalid transform scale for entity {} when creating sphere: {},{},{} - skipping",
                (uint64_t)entity.GetUUID(), tr.world.scale.x, tr.world.scale.y, tr.world.scale.z);
            return;
        }

        float maxAxis = glm::compMax(tr.world.scale);
        float effectiveRadius = col.radius * maxAxis;
        if (effectiveRadius <= 0.0f)
        {
            LOG_WARN("[Jolt Physics] Sphere collider for entity {} has non-positive radius {} - skipping",
                (uint64_t)entity.GetUUID(), effectiveRadius);
            return;
        }

        JPH::SphereShapeSettings shapeSettings(effectiveRadius);
        JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
        if (!shapeResult.IsValid() || shapeResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", shapeResult.GetError());
            return;
        }
        JPH::ShapeRefC shape = shapeResult.Get();

        JPH::RotatedTranslatedShapeSettings offsetSettings(GlmToJoltVec3(col.center * maxAxis), JPH::Quat::sIdentity(), shape.GetPtr());
        JPH::ShapeSettings::ShapeResult offsetResult = offsetSettings.Create();
        if (!offsetResult.IsValid() || offsetResult.HasError())
        {
            LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", offsetResult.GetError());
            return;
        }
        shape = offsetResult.Get();

        JPH::BodyCreationSettings bodySettings = CreateBody(shape, rb, tr.world.translation, tr.world.rotation);

        if (rb.body)
        {
            LOG_WARN("[Jolt Physics] Entity {} already has a body when creating sphere - removing existing body", (uint64_t)entity.GetUUID());
            if (m_BodyInterface)
            {
                m_BodyInterface->RemoveBody(rb.body->GetID());
                m_BodyInterface->DestroyBody(rb.body->GetID());
            }
            rb.body = nullptr;
        }

        JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
        if (body)
        {
            JPH::BodyID bodyId = body->GetID();
            m_BodyInterface->AddBody(bodyId, JPH::EActivation::Activate);
            m_BodyInterface->SetFriction(bodyId, col.friction);
            m_BodyInterface->SetRestitution(bodyId, col.restitution);
            body->SetUserData((uint64_t)entity.GetUUID());
            rb.body = body;
        }

        col.shape = (void *)shape.GetPtr();
    }

    void JoltScene::CreateMeshCollider(Entity entity)
    {
        auto &tr = entity.GetComponent<TransformComponent>();
        auto &rb = entity.GetComponent<RigidbodyComponent>();
        auto &col = entity.GetComponent<MeshColliderComponent>();

        if (col.vertices.empty())
        {
            LOG_WARN("[Jolt Physics] MeshCollider has no vertices, skipping creation");
            return;
        }

        JPH::ShapeRefC shape;

        if (col.convex)
        {
            // Create convex hull shape
            JPH::Array<JPH::Vec3> vertices;
            vertices.reserve(col.vertices.size());

            for (const auto& vertex : col.vertices)
            {
                vertices.push_back(GlmToJoltVec3(vertex));
            }

            JPH::ConvexHullShapeSettings shapeSettings(vertices);
            JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
            if (!shapeResult.IsValid() || shapeResult.HasError())
            {
                LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", shapeResult.GetError());
                return;
            }
            shape = shapeResult.Get();
        }
        else
        {
            // Create triangle mesh shape
            JPH::TriangleList triangles;

            if (col.indices.empty())
            {
                // Generate triangles from vertices (assuming they're ordered as triangles)
                if (col.vertices.size() % 3 == 0)
                {
                    for (size_t i = 0; i < col.vertices.size(); i += 3)
                    {
                        JPH::Triangle triangle(
                            GlmToJoltVec3(col.vertices[i]),
                            GlmToJoltVec3(col.vertices[i + 1]),
                            GlmToJoltVec3(col.vertices[i + 2])
                        );
                        triangles.push_back(triangle);
                    }
                }
                else
                {
                    LOG_ERROR("[Jolt Physics] MeshCollider vertices count is not divisible by 3 and no indices provided");
                    return;
                }
            }
            else
            {
                // Use indices to create triangles
                for (size_t i = 0; i < col.indices.size(); i += 3)
                {
                    if (i + 2 < col.indices.size() &&
                        col.indices[i] < col.vertices.size() &&
                        col.indices[i + 1] < col.vertices.size() &&
                        col.indices[i + 2] < col.vertices.size())
                    {
                        JPH::Triangle triangle(
                            GlmToJoltVec3(col.vertices[col.indices[i]]),
                            GlmToJoltVec3(col.vertices[col.indices[i + 1]]),
                            GlmToJoltVec3(col.vertices[col.indices[i + 2]])
                        );
                        triangles.push_back(triangle);
                    }
                }
            }

            if (triangles.empty())
            {
                LOG_ERROR("[Jolt Physics] No valid triangles could be created from MeshCollider data");
                return;
            }

            JPH::MeshShapeSettings shapeSettings(triangles);
            JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
            if (!shapeResult.IsValid() || shapeResult.HasError())
            {
                LOG_ASSERT(false, "[Jolt Physics] Invalid shape settings! {}", shapeResult.GetError());
                return;
            }

            shape = shapeResult.Get();
        }

        if (!shape)
        {
            LOG_ERROR("[Jolt Physics] Failed to create mesh shape");
            return;
        }

        JPH::BodyCreationSettings bodySettings = CreateBody(shape, rb, tr.world.translation, tr.world.rotation);

        JPH::Body *body = m_BodyInterface->CreateBody(bodySettings);
        if (body)
        {
            JPH::BodyID bodyId = body->GetID();
            m_BodyInterface->AddBody(bodyId, JPH::EActivation::Activate);
            m_BodyInterface->SetFriction(bodyId, col.friction);
            m_BodyInterface->SetRestitution(bodyId, col.restitution);
            body->SetUserData((uint64_t)entity.GetUUID());
            rb.body = body;
        }

        col.shape = (void *)shape.GetPtr();
    }

    void JoltScene::AddForce(const JPH::Body &body, const glm::vec3 &force)
    {
        m_BodyInterface->AddForce(body.GetID(), GlmToJoltVec3(force));
    }

    void JoltScene::AddTorque(const JPH::Body &body, const glm::vec3 &torque)
    {
        m_BodyInterface->AddTorque(body.GetID(), GlmToJoltVec3(torque));
    }

    void JoltScene::AddForceAndTorque(const JPH::Body &body, const glm::vec3 &force, const glm::vec3 &torque)
    {
        m_BodyInterface->AddForceAndTorque(body.GetID(), GlmToJoltVec3(force), GlmToJoltVec3(torque));
    }

    void JoltScene::AddAngularImpulse(const JPH::Body &body, const glm::vec3 &impulse)
    {
        m_BodyInterface->AddAngularImpulse(body.GetID(), GlmToJoltVec3(impulse));
    }

    void JoltScene::ActivateBody(const JPH::Body &body)
    {
        m_BodyInterface->ActivateBody(body.GetID());
    }

    void JoltScene::DeactivateBody(const JPH::Body &body)
    {
        m_BodyInterface->DeactivateBody(body.GetID());
    }

    void JoltScene::DestroyBody(const JPH::Body &body)
    {
        m_BodyInterface->DestroyBody(body.GetID());
    }

    bool JoltScene::IsActive(const JPH::Body &body)
    {
        return m_BodyInterface->IsActive(body.GetID());
    }

    void JoltScene::MoveKinematic(const JPH::Body &body, const glm::vec3 &targetPosition, const glm::vec3 &targetRotation, float deltaTime)
    {
        m_BodyInterface->MoveKinematic(body.GetID(), GlmToJoltVec3(targetPosition), GlmToJoltQuat(targetRotation), deltaTime);
    }

    void JoltScene::AddImpulse(const JPH::Body &body, const glm::vec3 &impulse)
    {
        m_BodyInterface->AddImpulse(body.GetID(), GlmToJoltVec3(impulse));
    }

    void JoltScene::AddLinearVelocity(const JPH::Body &body, const glm::vec3 &velocity)
    {
        m_BodyInterface->AddLinearVelocity(body.GetID(), GlmToJoltVec3(velocity));
    }

    void JoltScene::SetPosition(const JPH::Body &body, const glm::vec3 &position, bool activate)
    {
        m_BodyInterface->SetPosition(body.GetID(), GlmToJoltVec3(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    void JoltScene::SetEulerAngleRotation(const JPH::Body &body, const glm::vec3 &rotation, bool activate)
    {
        m_BodyInterface->SetRotation(body.GetID(), GlmToJoltQuat(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    void JoltScene::SetRotation(const JPH::Body &body, const glm::quat &rotation, bool activate)
    {
        m_BodyInterface->SetRotation(body.GetID(), GlmToJoltQuat(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    void JoltScene::SetLinearVelocity(const JPH::Body &body, const glm::vec3 &vel)
    {
        m_BodyInterface->SetLinearVelocity(body.GetID(), GlmToJoltVec3(vel));
    }

    void JoltScene::SetFriction(const JPH::Body &body, float value)
    {
        m_BodyInterface->SetFriction(body.GetID(), value);
    }

    void JoltScene::SetRestitution(const JPH::Body &body, float value)
    {
        m_BodyInterface->SetRestitution(body.GetID(), value);
    }

    void JoltScene::SetGravityFactor(const JPH::Body &body, float value)
    {
        m_BodyInterface->SetGravityFactor(body.GetID(), value);
    }

    float JoltScene::GetRestitution(const JPH::Body &body)
    {
        return m_BodyInterface->GetRestitution(body.GetID());
    }

    float JoltScene::GetFriction(const JPH::Body &body)
    {
        return m_BodyInterface->GetFriction(body.GetID());
    }

    float JoltScene::GetGravityFactor(const JPH::Body &body)
    {
        return m_BodyInterface->GetGravityFactor(body.GetID());
    }

    glm::vec3 JoltScene::GetPosition(const JPH::Body &body)
    {
        return JoltToGlmVec3(m_BodyInterface->GetPosition(body.GetID()));
    }

    glm::vec3 JoltScene::GetEulerAngles(const JPH::Body &body)
    {
        return glm::eulerAngles(JoltToGlmQuat(m_BodyInterface->GetRotation(body.GetID())));
    }

    glm::quat JoltScene::GetRotation(const JPH::Body &body)
    {
        return JoltToGlmQuat(m_BodyInterface->GetRotation(body.GetID()));
    }

    glm::vec3 JoltScene::GetCenterOfMassPosition(const JPH::Body &body)
    {
        return JoltToGlmVec3(m_BodyInterface->GetCenterOfMassPosition(body.GetID()));
    }

    glm::vec3 JoltScene::GetLinearVelocity(const JPH::Body &body)
    {
        return JoltToGlmVec3(m_BodyInterface->GetLinearVelocity(body.GetID()));
    }

    void JoltScene::SetMaxLinearVelocity(JPH::Body &body, float max)
    {
        body.GetMotionProperties()->SetMaxLinearVelocity(max);
    }

    void JoltScene::SetMaxAngularVelocity(JPH::Body &body, float max)
    {
        body.GetMotionProperties()->SetMaxAngularVelocity(max);
    }

    JoltRaycastHit JoltScene::Raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance)
    {
        JoltRaycastHit result;

        if (!m_BodyInterface)
            return result;

        // Build the ray
        JPH::Vec3 rayOrigin = GlmToJoltVec3(origin);
        JPH::Vec3 rayDir = GlmToJoltVec3(glm::normalize(direction) * maxDistance);

        JPH::RRayCast ray { JPH::RVec3(rayOrigin), rayDir };

        JPH::RayCastResult hit;
        const JPH::NarrowPhaseQuery &narrowPhase = m_PhysicsSystem.GetNarrowPhaseQuery();

        if (narrowPhase.CastRay(ray, hit))
        {
            result.hit = true;
            result.bodyId = hit.mBodyID;
            result.fraction = hit.mFraction;

            // Compute world-space hit point
            JPH::Vec3 hitPos = rayOrigin + rayDir * hit.mFraction;
            result.hitPoint = JoltToGlmVec3(hitPos);

            // Retrieve the surface normal at the hit sub-shape
            JPH::BodyLockRead bodyLock(m_PhysicsSystem.GetBodyLockInterface(), hit.mBodyID);
            if (bodyLock.Succeeded())
            {
                const JPH::Body &body = bodyLock.GetBody();
                JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, JPH::RVec3(hitPos));
                result.hitNormal = JoltToGlmVec3(normal);
            }
        }

        return result;
    }

    std::vector<JoltCollisionEvent> JoltScene::DrainCollisionEvents()
    {
        if (auto *listener = dynamic_cast<JoltContactListener *>(s_JoltInstance->contactListener.get()))
        {
            return listener->DrainEvents();
        }
        return {};
    }

	std::vector<JoltBodyActivationEvent> JoltScene::DrainActivationEvents()
	{
		if (auto *listener = dynamic_cast<JoltBodyActivationListener *>(s_JoltInstance->bodyActivationListener.get()))
		{
			return listener->DrainEvents();
		}
		return {};
	}

	JPH::uint64 JoltScene::GetUserData(const JPH::BodyID &bodyId)
    {
        return m_BodyInterface->GetUserData(bodyId);
    }
}
