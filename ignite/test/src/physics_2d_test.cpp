// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>
#include <filesystem>
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/project/project.hpp"

using namespace ignite;

static Ref<Project> CreateTestProject(const std::string &projectName)
{
    std::filesystem::path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    std::filesystem::path projectDir = testResourcesRoot / "temp" / projectName;

    if (std::filesystem::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    std::filesystem::create_directories(projectDir);

    ProjectInfo info;
    info.name = projectName;
    info.filepath = projectDir / (projectName + ".ixproj");
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    return Project::Create(info);
}

// ===============================================
// Physics2D Core Tests
// ===============================================

TEST(Physics2D, CreateAndDestroyWorld)
{
    physics::Physics2D physics;

    // World should be created in constructor
    EXPECT_TRUE(b2World_IsValid(physics.GetWorldId()));

    physics.SimulationStop();
    EXPECT_FALSE(b2World_IsValid(physics.GetWorldId()));

    physics.SimulationStart();
    EXPECT_TRUE(b2World_IsValid(physics.GetWorldId()));
}

TEST(Physics2D, CreateStaticBody)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_staticBody;
    bodyDef.position = {0.0f, 0.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    EXPECT_TRUE(physics.IsValidBody(bodyId));

    physics.DestroyBody(bodyId);
    EXPECT_FALSE(physics.IsValidBody(bodyId));
}

TEST(Physics2D, CreateDynamicBody)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {10.0f, 5.0f};
    bodyDef.linearVelocity = {1.0f, 2.0f};
    bodyDef.angularVelocity = 0.5f;
    bodyDef.gravityScale = 1.0f;
    bodyDef.linearDamping = 0.6f;
    bodyDef.angularDamping = 0.2f;

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    EXPECT_TRUE(physics.IsValidBody(bodyId));

    // Verify properties
    glm::vec2 pos = physics.GetPosition(bodyId);
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 5.0f);

    glm::vec2 vel = physics.GetLinearVelocity(bodyId);
    EXPECT_FLOAT_EQ(vel.x, 1.0f);
    EXPECT_FLOAT_EQ(vel.y, 2.0f);

    EXPECT_FLOAT_EQ(physics.GetAngularVelocity(bodyId), 0.5f);

    physics.DestroyBody(bodyId);
}

TEST(Physics2D, CreateKinematicBody)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_kinematicBody;
    bodyDef.position = {-5.0f, 3.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    EXPECT_TRUE(physics.IsValidBody(bodyId));

    physics.SetBodyType(bodyId, b2_dynamicBody);
    physics.DestroyBody(bodyId);
}

TEST(Physics2D, CreateBodyWithBoxCollider)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    // Create body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 0.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Create box collider
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    shapeDef.material.friction = 0.5f;
    shapeDef.material.restitution = 0.3f;

    b2Polygon box = b2MakeBox(1.0f, 1.0f);

    b2ShapeId shapeId = physics.CreateBoxCollider(bodyId, shapeDef, box);
    EXPECT_TRUE(b2Shape_IsValid(shapeId));

    // Verify mass was computed
    float mass = physics.GetMass(bodyId);
    EXPECT_GT(mass, 0.0f);

    physics.DestroyShape(shapeId, true);
    physics.DestroyBody(bodyId);
}

TEST(Physics2D, CreateBodyWithCircleCollider)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    // Create body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 10.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Create circle collider
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 2.0f;
    shapeDef.material.friction = 0.3f;
    shapeDef.material.restitution = 0.5f;
    shapeDef.isSensor = false;

    b2Circle circle = {};
    circle.center = {0.0f, 0.0f};
    circle.radius = 0.5f;

    b2ShapeId shapeId = physics.CreateCircleCollider(bodyId, shapeDef, circle);
    EXPECT_TRUE(b2Shape_IsValid(shapeId));

    physics.DestroyShape(shapeId, true);
    physics.DestroyBody(bodyId);
}

TEST(Physics2D, CreateBodyWithMultipleColliders)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 0.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Add box collider
    b2ShapeDef boxShapeDef = b2DefaultShapeDef();
    boxShapeDef.density = 1.0f;
    b2Polygon box = b2MakeOffsetBox(0.5f, 0.5f, {-0.5f, 0.0f}, b2MakeRot(0.0f));
    b2ShapeId boxShapeId = physics.CreateBoxCollider(bodyId, boxShapeDef, box);
    EXPECT_TRUE(b2Shape_IsValid(boxShapeId));

    // Add circle collider
    b2ShapeDef circleShapeDef = b2DefaultShapeDef();
    circleShapeDef.density = 1.0f;
    b2Circle circle = {{0.5f, 0.0f}, 0.3f};
    b2ShapeId circleShapeId = physics.CreateCircleCollider(bodyId, circleShapeDef, circle);
    EXPECT_TRUE(b2Shape_IsValid(circleShapeId));

    physics.DestroyShape(boxShapeId, true);
    physics.DestroyShape(circleShapeId, true);
    physics.DestroyBody(bodyId);
}

TEST(Physics2D, SimulationStep)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    // Create a dynamic body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 10.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Add a collider so the body has mass
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    b2Circle circle = {{0.0f, 0.0f}, 0.5f};
    physics.CreateCircleCollider(bodyId, shapeDef, circle);

    // Simulate a few steps
    for (int i = 0; i < 10; ++i)
    {
        physics.Simulate(1.0f / 60.0f);
    }

    // Body should have fallen (assuming default gravity)
    glm::vec2 pos = physics.GetPosition(bodyId);
    EXPECT_LT(pos.y, 10.0f); // Y should have decreased due to gravity

    physics.DestroyBody(bodyId);
}

TEST(Physics2D, ApplyForcesAndImpulses)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 0.0f};
    bodyDef.gravityScale = 0.0f; // Disable gravity for this test

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Add collider
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    b2Polygon box = b2MakeBox(1.0f, 1.0f);
    physics.CreateBoxCollider(bodyId, shapeDef, box);

    // Test linear impulse
    physics.ApplyLinearImpulseToCenter(bodyId, {10.0f, 5.0f}, true);
    glm::vec2 vel = physics.GetLinearVelocity(bodyId);
    EXPECT_GT(vel.x, 0.0f);
    EXPECT_GT(vel.y, 0.0f);

    // Test force application
    physics.SetLinearVelocity(bodyId, {0.0f, 0.0f});
    physics.ApplyForceToCenter(bodyId, {100.0f, 0.0f}, true);
    physics.Simulate(1.0f / 60.0f);
    vel = physics.GetLinearVelocity(bodyId);
    EXPECT_GT(vel.x, 0.0f);

    // Test torque
    physics.ApplyTorque(bodyId, 10.0f, true);
    physics.Simulate(1.0f / 60.0f);
    EXPECT_NE(physics.GetAngularVelocity(bodyId), 0.0f);

    physics.DestroyBody(bodyId);
}

TEST(Physics2D, BodyActivationState)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 0.0f};
    bodyDef.isAwake = true;
    bodyDef.isEnabled = true;

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Test deactivation/activation
    physics.DeactivateBody(bodyId);
    physics.ActivateBody(bodyId);

    physics.SetAwake(bodyId, false);
    physics.SetAwake(bodyId, true);

    physics.DestroyBody(bodyId);
}

TEST(Physics2D, BodyMotionLocks)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 0.0f};
    bodyDef.gravityScale = 0.0f;

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    // Add collider
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    b2Polygon box = b2MakeBox(0.5f, 0.5f);
    physics.CreateBoxCollider(bodyId, shapeDef, box);

    // Lock rotation
    physics.SetMotionLock(bodyId, false, false, true);

    // Apply torque - should have no effect with locked rotation
    physics.ApplyTorque(bodyId, 100.0f, true);
    physics.Simulate(1.0f / 60.0f);

    // Angular velocity should remain near zero with locked rotation
    EXPECT_NEAR(physics.GetAngularVelocity(bodyId), 0.0f, 0.001f);

    physics.DestroyBody(bodyId);
}

TEST(Physics2D, BulletBody)
{
    physics::Physics2D physics;
    ASSERT_TRUE(b2World_IsValid(physics.GetWorldId()));

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {0.0f, 0.0f};

    b2BodyId bodyId = physics.CreateBody(bodyDef);
    ASSERT_TRUE(physics.IsValidBody(bodyId));

    EXPECT_FALSE(physics.IsBullet(bodyId));

    physics.SetBullet(bodyId, true);
    EXPECT_TRUE(physics.IsBullet(bodyId));

    physics.SetBullet(bodyId, false);
    EXPECT_FALSE(physics.IsBullet(bodyId));

    physics.DestroyBody(bodyId);
}

// ===============================================
// Rigidbody2DComponent Tests (Scene Integration)
// ===============================================

TEST(Physics2DComponent, RigidbodyComponentDefaults)
{
    Ref<Project> project = CreateTestProject("RigidbodyDefaultsProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "TestRigidbody", EntityType_Node);

    EXPECT_FALSE(entity.HasComponent<Rigidbody2DComponent>());

    auto &rb = entity.AddComponent<Rigidbody2DComponent>();
    EXPECT_TRUE(entity.HasComponent<Rigidbody2DComponent>());

    // Check default values
    EXPECT_EQ(rb.bodyType, physics::BodyType::Static);
    EXPECT_EQ(rb.linearVelocity, glm::vec2(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(rb.angularVelocity, 0.0f);
    EXPECT_FLOAT_EQ(rb.gravityScale, 1.0f);
    EXPECT_FLOAT_EQ(rb.linearDamping, 0.6f);
    EXPECT_FLOAT_EQ(rb.angularDamping, 0.2f);
    EXPECT_TRUE(rb.isAwake);
    EXPECT_TRUE(rb.isEnabled);
}

TEST(Physics2DComponent, BoxColliderComponentDefaults)
{
    Ref<Project> project = CreateTestProject("BoxColliderDefaultsProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "TestBoxCollider", EntityType_Node);

    auto &bc = entity.AddComponent<BoxCollider2DComponent>();

    EXPECT_EQ(bc.size, glm::vec2(0.5f, 0.5f));
    EXPECT_EQ(bc.offset, glm::vec2(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(bc.restitution, 0.1f);
    EXPECT_FLOAT_EQ(bc.friction, 0.5f);
    EXPECT_FLOAT_EQ(bc.density, 1.0f);
    EXPECT_FALSE(bc.isSensor);
}

TEST(Physics2DComponent, CircleColliderComponentDefaults)
{
    Ref<Project> project = CreateTestProject("CircleColliderDefaultsProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "TestCircleCollider", EntityType_Node);

    auto &cc = entity.AddComponent<CircleCollider2DComponent>();

    EXPECT_EQ(cc.center, glm::vec2(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(cc.radius, 0.5f);
    EXPECT_FLOAT_EQ(cc.restitution, 0.1f);
    EXPECT_FLOAT_EQ(cc.friction, 0.5f);
    EXPECT_FLOAT_EQ(cc.density, 1.0f);
    EXPECT_FALSE(cc.isSensor);
}

TEST(Physics2DComponent, CreatePhysicsBodyOnSceneStart)
{
    Ref<Project> project = CreateTestProject("PhysicsBodyCreateProject");
    ASSERT_NE(project, nullptr);
    ASSERT_NE(project->GetPhysics2D(), nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "PhysicsBody", EntityType_Node);

    // Add transform
    auto &tr = entity.GetComponent<TransformComponent>();
    tr.local.translation = glm::vec3(5.0f, 10.0f, 0.0f);

    // Add rigidbody
    auto &rb = entity.AddComponent<Rigidbody2DComponent>();
    rb.bodyType = physics::BodyType::Dynamic;
    rb.linearVelocity = glm::vec2(1.0f, 2.0f);
    rb.angularVelocity = 0.5f;

    // Add box collider
    auto &bc = entity.AddComponent<BoxCollider2DComponent>();
    bc.size = glm::vec2(1.0f, 1.0f);
    bc.density = 2.0f;
    bc.friction = 0.7f;

    // Add circle collider
    auto &cc = entity.AddComponent<CircleCollider2DComponent>();
    cc.radius = 0.5f;
    cc.density = 1.5f;

    // Start the scene - this should create the physics body
    scene->OnStart(ESceneState::Simulate);

    // Body should now be valid
    EXPECT_TRUE(b2Body_IsValid(rb.bodyId));
    EXPECT_TRUE(b2Shape_IsValid(bc.shapeId));
    EXPECT_TRUE(b2Shape_IsValid(cc.shapeId));

    // Stop the scene - this should destroy the physics body
    scene->OnStop();

    EXPECT_FALSE(b2Body_IsValid(rb.bodyId));
    EXPECT_FALSE(b2Shape_IsValid(bc.shapeId));
    EXPECT_FALSE(b2Shape_IsValid(cc.shapeId));
}

TEST(Physics2DComponent, PhysicsBodyWithZeroScale)
{
    Ref<Project> project = CreateTestProject("ZeroScaleProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "ZeroScaleBody", EntityType_Node);

    // Set zero scale - this could cause issues with collider creation
    auto &tr = entity.GetComponent<TransformComponent>();
    tr.local.scale = glm::vec3(0.0f, 0.0f, 0.0f);

    auto &rb = entity.AddComponent<Rigidbody2DComponent>();
    rb.bodyType = physics::BodyType::Dynamic;

    auto &bc = entity.AddComponent<BoxCollider2DComponent>();
    bc.size = glm::vec2(1.0f, 1.0f);

    // This should not crash or assert
    scene->OnStart(ESceneState::Simulate);

    // Body should be created (collider may have minimum size due to epsilon clamping)
    EXPECT_TRUE(b2Body_IsValid(rb.bodyId));

    scene->OnStop();
}

TEST(Physics2DComponent, PhysicsBodyWithNegativeScale)
{
    Ref<Project> project = CreateTestProject("NegativeScaleProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "NegativeScaleBody", EntityType_Node);

    // Set negative scale
    auto &tr = entity.GetComponent<TransformComponent>();
    tr.local.scale = glm::vec3(-1.0f, -2.0f, 1.0f);

    auto &rb = entity.AddComponent<Rigidbody2DComponent>();
    rb.bodyType = physics::BodyType::Dynamic;

    auto &bc = entity.AddComponent<BoxCollider2DComponent>();
    bc.size = glm::vec2(1.0f, 0.5f);

    // This should handle negative scale gracefully (use abs)
    scene->OnStart(ESceneState::Simulate);

    EXPECT_TRUE(b2Body_IsValid(rb.bodyId));
    EXPECT_TRUE(b2Shape_IsValid(bc.shapeId));

    scene->OnStop();
}

TEST(Physics2DComponent, MultipleBodiesSimulation)
{
    Ref<Project> project = CreateTestProject("MultipleBodiesProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());

    // Create ground (static)
    Entity ground = SceneManager::CreateEntity(scene.get(), "Ground", EntityType_Node);
    auto &groundTr = ground.GetComponent<TransformComponent>();
    groundTr.local.translation = glm::vec3(0.0f, -5.0f, 0.0f);

    auto &groundRb = ground.AddComponent<Rigidbody2DComponent>();
    groundRb.bodyType = physics::BodyType::Static;

    auto &groundBc = ground.AddComponent<BoxCollider2DComponent>();
    groundBc.size = glm::vec2(10.0f, 1.0f);

    // Create multiple dynamic bodies
    std::vector<Entity> bodies;
    for (int i = 0; i < 5; ++i)
    {
        Entity body = SceneManager::CreateEntity(scene.get(), "Body" + std::to_string(i), EntityType_Node);
        auto &tr = body.GetComponent<TransformComponent>();
        tr.local.translation = glm::vec3(i * 2.0f - 4.0f, 5.0f, 0.0f);

        auto &rb = body.AddComponent<Rigidbody2DComponent>();
        rb.bodyType = physics::BodyType::Dynamic;

        auto &bc = body.AddComponent<BoxCollider2DComponent>();
        bc.size = glm::vec2(0.5f, 0.5f);

        bodies.push_back(body);
    }

    scene->OnStart(ESceneState::Simulate);

    // Simulate for a while
    for (int i = 0; i < 60; ++i)
    {
        scene->OnUpdateRuntimeSimulate(1.0f / 60.0f);
    }

    // Bodies should have fallen
    for (auto &body : bodies)
    {
        auto &tr = body.GetComponent<TransformComponent>();
        // Bodies should have moved down due to gravity
        EXPECT_LT(tr.world.translation.y, 5.0f);
    }

    scene->OnStop();
}

TEST(Physics2DComponent, SensorCollider)
{
    Ref<Project> project = CreateTestProject("SensorProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());

    Entity sensor = SceneManager::CreateEntity(scene.get(), "Sensor", EntityType_Node);
    auto &rb = sensor.AddComponent<Rigidbody2DComponent>();
    rb.bodyType = physics::BodyType::Static;

    auto &bc = sensor.AddComponent<BoxCollider2DComponent>();
    bc.size = glm::vec2(2.0f, 2.0f);
    bc.isSensor = true;

    scene->OnStart(ESceneState::Simulate);

    EXPECT_TRUE(b2Body_IsValid(rb.bodyId));
    EXPECT_TRUE(b2Shape_IsValid(bc.shapeId));

    scene->OnStop();
}
