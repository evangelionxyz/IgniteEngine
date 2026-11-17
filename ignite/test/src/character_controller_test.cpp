// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>
#include <filesystem>
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/serializer/entity_serializer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/physics/3d/jolt/jolt_physics.hpp"
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

TEST(CharacterController, ComponentDefaultsAndProperties)
{
    Ref<Project> project = CreateTestProject("CCDefaultsProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "PlayerCharacter", EntityType_Node);

    EXPECT_FALSE(entity.HasComponent<CharacterControllerComponent>());

    auto &cc = entity.AddComponent<CharacterControllerComponent>();
    EXPECT_TRUE(entity.HasComponent<CharacterControllerComponent>());

    EXPECT_FLOAT_EQ(cc.radius, 0.5f);
    EXPECT_FLOAT_EQ(cc.height, 2.0f);
    EXPECT_FLOAT_EQ(cc.maxStepHeight, 0.4f);
    EXPECT_FLOAT_EQ(cc.maxSlopeAngle, 45.0f);
    EXPECT_FLOAT_EQ(cc.mass, 80.0f);
    EXPECT_FLOAT_EQ(cc.friction, 0.2f);
    EXPECT_FLOAT_EQ(cc.gravityFactor, 1.0f);
    EXPECT_EQ(cc.center, glm::vec3(0.0f));
    EXPECT_EQ(cc.up, glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(CharacterController, SerializationRoundtrip)
{
    Ref<Project> project = CreateTestProject("CCSerialProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity originalEntity = SceneManager::CreateEntity(scene.get(), "TestCharacter", EntityType_Node);

    auto &cc = originalEntity.AddComponent<CharacterControllerComponent>();
    cc.radius = 0.75f;
    cc.height = 1.8f;
    cc.maxStepHeight = 0.5f;
    cc.maxSlopeAngle = 50.0f;
    cc.mass = 90.0f;
    cc.friction = 0.3f;
    cc.gravityFactor = 1.5f;
    cc.center = glm::vec3(0.1f, 0.2f, 0.3f);
    cc.up = glm::vec3(0.0f, 1.0f, 0.0f);
    cc.linearVelocity = glm::vec3(1.0f, 2.0f, 3.0f);

    std::filesystem::path tempPath = vfs::GetExecutableDirectory() / "test_character.json";
    Serializer sr(tempPath);
    EntitySerializer::SerializeEntity(sr, originalEntity);
    sr.Serialize();

    JsonNode rootNode = Serializer::Deserialize(tempPath);

    Entity deserializedEntity = EntitySerializer::DeserializeEntity(rootNode, scene.get(), project.get());

    EXPECT_TRUE(deserializedEntity.HasComponent<CharacterControllerComponent>());
    auto &desCC = deserializedEntity.GetComponent<CharacterControllerComponent>();

    EXPECT_FLOAT_EQ(desCC.radius, 0.75f);
    EXPECT_FLOAT_EQ(desCC.height, 1.8f);
    EXPECT_FLOAT_EQ(desCC.maxStepHeight, 0.5f);
    EXPECT_FLOAT_EQ(desCC.maxSlopeAngle, 50.0f);
    EXPECT_FLOAT_EQ(desCC.mass, 90.0f);
    EXPECT_FLOAT_EQ(desCC.friction, 0.3f);
    EXPECT_FLOAT_EQ(desCC.gravityFactor, 1.5f);
    EXPECT_FLOAT_EQ(desCC.center.x, 0.1f);
    EXPECT_FLOAT_EQ(desCC.center.y, 0.2f);
    EXPECT_FLOAT_EQ(desCC.center.z, 0.3f);
    EXPECT_FLOAT_EQ(desCC.linearVelocity.x, 1.0f);
    EXPECT_FLOAT_EQ(desCC.linearVelocity.y, 2.0f);
    EXPECT_FLOAT_EQ(desCC.linearVelocity.z, 3.0f);

    if (std::filesystem::exists(tempPath))
    {
        std::filesystem::remove(tempPath.string());
    }
}

TEST(CharacterController, PhysicsSimulationLifecycle)
{
    Ref<Project> project = CreateTestProject("CCSimProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "SimCharacter", EntityType_Node);
    auto &cc = entity.AddComponent<CharacterControllerComponent>();
    cc.radius = 0.5f;
    cc.height = 2.0f;

    auto &tr = entity.GetTransform();
    tr.local.translation = glm::vec3(0.0f, 10.0f, 0.0f);
    tr.world.translation = glm::vec3(0.0f, 10.0f, 0.0f);

    ignite::physics::Physics3D *physics3D = scene->GetPhysics3D();
    ASSERT_NE(physics3D, nullptr);

    scene->OnStart(ESceneState::Play);
    EXPECT_FALSE(cc.character.expired());

    // Simulate 10 frames of 1/60th seconds
    for (int i = 0; i < 10; ++i)
    {
        scene->OnUpdateRuntimeSimulate(1.0f / 60.0f);
    }

    scene->OnStop();
    EXPECT_TRUE(cc.character.expired());
}

TEST(CharacterController, ScaledPhysicsLifecycle)
{
    Ref<Project> project = CreateTestProject("CCScaledSimProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "ScaledCharacter", EntityType_Node);
    auto &cc = entity.AddComponent<CharacterControllerComponent>();
    cc.radius = 0.5f;
    cc.height = 2.0f;

    auto &tr = entity.GetTransform();
    tr.local.scale = glm::vec3(2.0f);
    tr.world.scale = glm::vec3(2.0f);

    scene->OnStart(ESceneState::Play);
    EXPECT_FALSE(cc.character.expired());

    for (int i = 0; i < 5; ++i)
    {
        scene->OnUpdateRuntimeSimulate(1.0f / 60.0f);
    }

    scene->OnStop();
    EXPECT_TRUE(cc.character.expired());
}

TEST(CharacterController, PhysicsMethodsAndMovement)
{
    Ref<Project> project = CreateTestProject("CCMethodsProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "MovementCharacter", EntityType_Node);
    auto &cc = entity.AddComponent<CharacterControllerComponent>();
    cc.radius = 0.5f;
    cc.height = 2.0f;
    cc.center = glm::vec3(0.0f, 1.0f, 0.0f);

    scene->OnStart(ESceneState::Play);
    auto charActor = cc.character.lock();
    ASSERT_NE(charActor, nullptr);

    // Test initial position & GroundNormal default
    charActor->SetPosition(glm::vec3(1.0f, 5.0f, 2.0f));
    EXPECT_NEAR(charActor->GetPosition().x, 1.0f, 1e-4f);
    EXPECT_NEAR(charActor->GetPosition().y, 5.0f, 1e-4f);
    EXPECT_NEAR(charActor->GetPosition().z, 2.0f, 1e-4f);

    glm::vec3 normal = charActor->GetGroundNormal();
    EXPECT_FLOAT_EQ(normal.y, 0.0f);

    // Test rotation
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    charActor->SetRotation(rot);
    EXPECT_NEAR(charActor->GetRotation().w, rot.w, 1e-4f);

    // Test movement
    charActor->Move(glm::vec3(2.0f, 0.0f, 0.0f), 1.0f / 60.0f);
    EXPECT_FALSE(cc.character.expired());

    scene->OnStop();
}

TEST(CharacterController, AppliesConfiguredGravityFactorWhileFalling)
{
    Ref<Project> project = CreateTestProject("CCGravityProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity entity = SceneManager::CreateEntity(scene.get(), "FallingCharacter", EntityType_Node);
    auto &cc = entity.AddComponent<CharacterControllerComponent>();
    cc.gravityFactor = 2.24f;

    auto &tr = entity.GetTransform();
    tr.local.translation = glm::vec3(0.0f, 100.0f, 0.0f);
    tr.world.translation = glm::vec3(0.0f, 100.0f, 0.0f);

    scene->OnStart(ESceneState::Play);
    auto character = cc.character.lock();
    ASSERT_NE(character, nullptr);

    constexpr float deltaTime = 1.0f / 60.0f;
    constexpr int frameCount = 30;
    for (int i = 0; i < frameCount; ++i)
        scene->OnUpdateRuntimeSimulate(deltaTime);

    const float expectedVelocity = -9.81f * 2.24f * deltaTime * frameCount;
    EXPECT_NEAR(character->GetLinearVelocity().y, expectedVelocity, 0.03f);

    scene->OnStop();
}

TEST(CharacterController, FollowsTranslatingKinematicGround)
{
    Ref<Project> project = CreateTestProject("CCMovingPlatformProject");
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    Entity platform = SceneManager::CreateEntity(scene.get(), "MovingPlatform", EntityType_Node);
    auto &platformBody = platform.AddComponent<RigidbodyComponent>();
    platformBody.bodyType = physics::BodyType::Kinematic;
    platformBody.useGravity = false;
    auto &platformCollider = platform.AddComponent<BoxColliderComponent>();
    platformCollider.scale = glm::vec3(5.0f, 0.5f, 5.0f);

    Entity player = SceneManager::CreateEntity(scene.get(), "PlatformCharacter", EntityType_Node);
    auto &cc = player.AddComponent<CharacterControllerComponent>();
    auto &playerTransform = player.GetTransform();
    playerTransform.local.translation = glm::vec3(0.0f, 1.5f, 0.0f);
    playerTransform.world.translation = playerTransform.local.translation;

    scene->OnStart(ESceneState::Play);
    auto character = cc.character.lock();
    auto kinematicBody = platformBody.dynamicActor.lock();
    ASSERT_NE(character, nullptr);
    ASSERT_NE(kinematicBody, nullptr);

    constexpr float deltaTime = 1.0f / 60.0f;
    for (int i = 0; i < 12; ++i)
        scene->OnUpdateRuntimeSimulate(deltaTime);
    ASSERT_TRUE(character->IsOnGround());
    EXPECT_TRUE(character->IsSupported());

    const float startingX = character->GetPosition().x;
    kinematicBody->SetLinearVelocity(glm::vec3(1.0f, 0.0f, 0.0f));
    for (int i = 0; i < 60; ++i)
        scene->OnUpdateRuntimeSimulate(deltaTime);

    EXPECT_TRUE(character->IsOnGround());
    EXPECT_TRUE(character->IsSupported());
    EXPECT_NEAR(character->GetGroundVelocity().x, 1.0f, 0.05f);
    EXPECT_GT(character->GetPosition().x, startingX + 0.8f);

    const float startingY = character->GetPosition().y;
    kinematicBody->SetLinearVelocity(glm::vec3(0.0f, 0.5f, 0.0f));
    for (int i = 0; i < 60; ++i)
        scene->OnUpdateRuntimeSimulate(deltaTime);

    EXPECT_TRUE(character->IsOnGround());
    EXPECT_NEAR(character->GetGroundVelocity().y, 0.5f, 0.05f);
    EXPECT_GT(character->GetPosition().y, startingY + 0.4f);

    scene->OnStop();
}
