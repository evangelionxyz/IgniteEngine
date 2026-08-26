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

    std::filesystem::path tempPath = vfs::GetExecutableDirectory() / "test_character.yaml";
    Serializer sr(tempPath);
    EntitySerializer::SerializeEntity(sr, originalEntity);
    sr.Serialize();

    YAML::Node rootNode = Serializer::Deserialize(tempPath);

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
