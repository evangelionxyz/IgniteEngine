// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite/core/types.hpp"
#include "ignite/project/input_mapping.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/project/project.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/core/signals/signals.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/animation/blend_space.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <format>

#include <gtest/gtest.h>

using namespace ignite;

// -------------------------------------------------
// BlendSpace 2D Evaluation & Serialization Tests
// -------------------------------------------------

TEST(BlendSpace, 2DEvaluationAndSerialization)
{
    ignite::Path filepath = "test-resources/temp/test_blendspace.bs2d";
    if (!ignite::Path::exists(filepath.parent_path()))
    {
        ignite::Path::create_directories(filepath.parent_path());
    }

    Ref<BlendSpace> bs = BlendSpace::Create();
    bs->axisXName = "Direction";
    bs->axisYName = "Speed";
    bs->axisMin = glm::vec2(-180.0f, 0.0f);
    bs->axisMax = glm::vec2(180.0f, 10.0f);

    BlendSpaceSample s1;
    s1.position = glm::vec2(0.0f, 0.0f);
    s1.SetAnimationHandle(AssetHandle(101));

    BlendSpaceSample s2;
    s2.position = glm::vec2(0.0f, 10.0f);
    s2.SetAnimationHandle(AssetHandle(102));

    BlendSpaceSample s3;
    s3.position = glm::vec2(-180.0f, 5.0f);
    s3.SetAnimationHandle(AssetHandle(103));

    BlendSpaceSample s4;
    s4.position = glm::vec2(180.0f, 5.0f);
    s4.SetAnimationHandle(AssetHandle(104));

    bs->samples.reserve(4);
    bs->samples.push_back(s1);
    bs->samples.push_back(s2);
    bs->samples.push_back(s3);
    bs->samples.push_back(s4);

    // Test input clamping
    glm::vec2 clamped = bs->ClampInput(glm::vec2(-250.0f, 15.0f));
    EXPECT_FLOAT_EQ(clamped.x, -180.0f);
    EXPECT_FLOAT_EQ(clamped.y, 10.0f);

    // Test exact sample point evaluation
    auto exactWeights = bs->Evaluate(glm::vec2(0.0f, 0.0f));
    ASSERT_FALSE(exactWeights.empty());
    EXPECT_EQ(exactWeights[0].GetAnimationAssetHandle(), AssetHandle(101));
    EXPECT_FLOAT_EQ(exactWeights[0].weight, 1.0f);

    // Test midpoint evaluation weights sum to 1
    auto midWeights = bs->Evaluate(glm::vec2(0.0f, 5.0f));
    ASSERT_FALSE(midWeights.empty());
    float sumWeights = 0.0f;
    for (const auto &w : midWeights)
    {
        sumWeights += w.weight;
    }
    EXPECT_NEAR(sumWeights, 1.0f, 0.001f);

    // Test serialization
    ASSERT_TRUE(bs->Serialize(filepath));

    // Test deserialization
    Ref<BlendSpace> loadedBs = BlendSpace::Deserialize(filepath);
    ASSERT_NE(loadedBs, nullptr);
    EXPECT_EQ(loadedBs->axisXName, "Direction");
    EXPECT_EQ(loadedBs->axisYName, "Speed");
    EXPECT_FLOAT_EQ(loadedBs->axisMin.x, -180.0f);
    EXPECT_FLOAT_EQ(loadedBs->axisMin.y, 0.0f);
    EXPECT_FLOAT_EQ(loadedBs->axisMax.x, 180.0f);
    EXPECT_FLOAT_EQ(loadedBs->axisMax.y, 10.0f);
    EXPECT_EQ(loadedBs->samples.size(), 4);
    EXPECT_EQ(loadedBs->samples[0].GetAnimationAssetHandle(), AssetHandle(101));
    EXPECT_EQ(loadedBs->samples[1].GetAnimationAssetHandle(), AssetHandle(102));
}

TEST(BlendSpace, NormalizedMultiSampleEvaluation)
{
    Ref<BlendSpace> bs = BlendSpace::Create();
    bs->axisXName = "Direction";
    bs->axisYName = "Speed";
    bs->axisMin = glm::vec2(-180.0f, 0.0f);
    bs->axisMax = glm::vec2(180.0f, 1.0f);

    // Idle samples at Y = 0
    BlendSpaceSample idle1; idle1.position = glm::vec2(-180.0f, 0.0f); idle1.SetAnimationHandle(AssetHandle(201));
    BlendSpaceSample idle2; idle2.position = glm::vec2(0.0f, 0.0f);    idle2.SetAnimationHandle(AssetHandle(202));
    BlendSpaceSample idle3; idle3.position = glm::vec2(180.0f, 0.0f);  idle3.SetAnimationHandle(AssetHandle(203));

    // Running samples at Y = 1
    BlendSpaceSample run1; run1.position = glm::vec2(-180.0f, 1.0f); run1.SetAnimationHandle(AssetHandle(301));
    BlendSpaceSample run2; run2.position = glm::vec2(0.0f, 1.0f);    run2.SetAnimationHandle(AssetHandle(302));
    BlendSpaceSample run3; run3.position = glm::vec2(180.0f, 1.0f);  run3.SetAnimationHandle(AssetHandle(303));

    bs->samples = { idle1, idle2, idle3, run1, run2, run3 };

    // Case 1: X = -180, Y = 0 -> 100% idle1 (201)
    auto weightsExact = bs->Evaluate(glm::vec2(-180.0f, 0.0f));
    ASSERT_EQ(weightsExact.size(), 1);
    EXPECT_EQ(weightsExact[0].GetAnimationAssetHandle(), AssetHandle(201));
    EXPECT_FLOAT_EQ(weightsExact[0].weight, 1.0f);

    // Case 2: X = -90, Y = 0 -> ONLY idle samples (201, 202, 203) contribute
    auto weightsMid = bs->Evaluate(glm::vec2(-90.0f, 0.0f));
    ASSERT_FALSE(weightsMid.empty());
    for (const auto &w : weightsMid)
    {
        uint64_t handleVal = static_cast<uint64_t>(w.GetAnimationAssetHandle());
        EXPECT_TRUE(handleVal == 201 || handleVal == 202 || handleVal == 203);
    }
}

// -------------------------------------------------
// AnimatorController Parameter & Motion Evaluation Tests
// -------------------------------------------------

TEST(AnimatorController, BlendSpaceParameterEvaluation)
{
    Ref<AnimatorController> controller = AnimatorController::Create();
    controller->params.push_back({ .name = "Direction", .floatVal = 0.0f, .type = AnimParam::Type::Float });
    controller->params.push_back({ .name = "Speed", .floatVal = 0.0f, .type = AnimParam::Type::Float });

    controller->SetParamFloat("Direction", -180.0f);
    controller->SetParamFloat("Speed", 5.0f);

    const AnimParam *dirParam = controller->GetParam("Direction");
    const AnimParam *speedParam = controller->GetParam("Speed");

    ASSERT_NE(dirParam, nullptr);
    ASSERT_NE(speedParam, nullptr);
    EXPECT_FLOAT_EQ(dirParam->floatVal, -180.0f);
    EXPECT_FLOAT_EQ(speedParam->floatVal, 5.0f);

    AnimState blendState;
    blendState.name = "WalkRunBlendSpace";
    blendState.SetMotion(AnimState::MotionType::BlendSpace, AssetHandle(505));
    controller->states.push_back(blendState);
    controller->defaultState = "WalkRunBlendSpace";

    EXPECT_EQ(controller->states.size(), 1);
    EXPECT_EQ(controller->states[0].GetMotionType(), AnimState::MotionType::BlendSpace);
    EXPECT_EQ(controller->states[0].GetMotionHandle(), AssetHandle(505));
}

// -------------------------------------------------
// AnimationMontage Notify Callback & Serialization Tests
// -------------------------------------------------

TEST(AnimationMontage, NotifyCallbackSerializationAndCompat)
{
    ignite::Path filepath = "test-resources/temp/test_montage_v2.montage";
    if (!ignite::Path::exists(filepath.parent_path()))
    {
        ignite::Path::create_directories(filepath.parent_path());
    }

    Ref<AnimationMontage> montage = CreateRef<AnimationMontage>();
    montage->name = "ComboAttackMontage";
    montage->SetAnimationHandle(AssetHandle(301));
    montage->SetSkeletonHandle(AssetHandle(401));

    montage->AddNotif("HitWindow", 0.2f, 0.6f);
    montage->AddNotifyCallback(0.35f, AnimationTimelineEvent::Action::ScriptCallback, "OnHitBoxActive");
    montage->AddNotifyCallback(0.70f, AnimationTimelineEvent::Action::Audio, "OnSlashSound");
    montage->SetMaskedJoints({ 1, 2, 3, 5, 8 });

    ASSERT_TRUE(montage->Serialize(filepath));

    Ref<AnimationMontage> loaded = AnimationMontage::Deserialize(filepath);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->name, "ComboAttackMontage");
    EXPECT_EQ(loaded->GetAnimationHandle(), AssetHandle(301));
    EXPECT_EQ(loaded->GetSkeletonHandle(), AssetHandle(401));

    EXPECT_EQ(loaded->GetAnimNotifies().size(), 1);
    EXPECT_EQ(loaded->GetNotifyCallbacks().size(), 2);

    EXPECT_FLOAT_EQ(loaded->GetNotifyCallbacks()[0].timestep, 0.35f);
    EXPECT_EQ(loaded->GetNotifyCallbacks()[0].actionType, AnimationTimelineEvent::Action::ScriptCallback);
    EXPECT_EQ(loaded->GetNotifyCallbacks()[0].callbackName, "OnHitBoxActive");

    EXPECT_FLOAT_EQ(loaded->GetNotifyCallbacks()[1].timestep, 0.70f);
    EXPECT_EQ(loaded->GetNotifyCallbacks()[1].actionType, AnimationTimelineEvent::Action::Audio);
    EXPECT_EQ(loaded->GetNotifyCallbacks()[1].callbackName, "OnSlashSound");

    EXPECT_EQ(loaded->GetMaskedJoints().size(), 5);
    EXPECT_EQ(loaded->GetMaskedJoints()[0], 1);
    EXPECT_EQ(loaded->GetMaskedJoints()[3], 5);
}

TEST(AnimationMontage, NotifyCallbackTimestepTrigger)
{
    AnimNotifyCallback cb(0.50f, AnimationTimelineEvent::Action::ScriptCallback, "OnStepTrigger");

    // Before trigger
    cb.OnUpdate(0.40f);
    EXPECT_FALSE(cb.IsActive());
    EXPECT_FALSE(cb.JustTriggered());

    // At trigger timestep
    cb.OnUpdate(0.50f);
    EXPECT_TRUE(cb.IsActive());
    EXPECT_TRUE(cb.JustTriggered());

    // Stay at trigger timestep (second frame)
    cb.OnUpdate(0.50f);
    EXPECT_TRUE(cb.IsActive());
    EXPECT_FALSE(cb.JustTriggered());

    // After trigger
    cb.OnUpdate(0.60f);
    EXPECT_FALSE(cb.IsActive());
    EXPECT_FALSE(cb.JustTriggered());
}

TEST(InputSerializer, SerializeDeserialize)
{
    ignite::Path filepath = "test-resources/temp/test_input_system.json";
    
    if (!ignite::Path::exists(filepath.parent_path()))
    {
        ignite::Path::create_directories(filepath.parent_path());
    }

    Ref<InputMapping> inputSystem = CreateRef<InputMapping>();
    
    // Add some input mappings
    inputSystem->MapKey(Key::Space, "Jump");
    inputSystem->MapKey(Key::W, "MoveForward");
    inputSystem->MapKey(Key::S, "MoveBackward");
    
    // Serialize the InputMapping to a file
    ASSERT_TRUE(inputSystem->Serialize(filepath));

    // Deserialize the InputMapping from the file
    Ref<InputMapping> deserializedInputMapping = InputMapping::Deserialize(filepath);
    ASSERT_NE(deserializedInputMapping, nullptr);
    
    // Check that the deserialized input mappings match the original
    EXPECT_EQ(deserializedInputMapping->m_KeyMappings.size(), inputSystem->m_KeyMappings.size());
    for (const auto &[key, action] : inputSystem->m_KeyMappings)
    {
        auto it = deserializedInputMapping->m_KeyMappings.find(key);
        ASSERT_NE(it, deserializedInputMapping->m_KeyMappings.end());
        EXPECT_EQ(it->second, action);
    }
}

// -------------------------------------------------
// Project Creation Test
// -------------------------------------------------
TEST(EngineTests, ProjectCreation)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/ProjectCreation";

    // Clean up any existing directory
    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "TestProject";
    info.filepath = projectDir / "TestProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    // Verify project directories were created
    EXPECT_TRUE(ignite::Path::exists(project->GetAssetDirectory()));
    EXPECT_TRUE(ignite::Path::exists(project->GetScriptsDirectory()));
    EXPECT_TRUE(ignite::Path::exists(project->GetScriptBinDirectory()));

    // Verify generated visual studio project files
    EXPECT_TRUE(ignite::Path::exists(project->GetSolutionFilepath()));
    EXPECT_TRUE(ignite::Path::exists(projectDir / "TestProject.csproj"));
    EXPECT_TRUE(ignite::Path::exists(projectDir / "Directory.Build.props"));
}

// -------------------------------------------------
// Asset Importer & Manager Test
// -------------------------------------------------
TEST(EngineTests, AssetManager)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/AssetManager";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "AssetManagerProject";
    info.filepath = projectDir / "AssetManagerProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

	AssetManager *assetManager = AssetManager::GetInstance();
    ASSERT_NE(assetManager, nullptr);
	assetManager->SetActiveProject(project);

    for (auto texFilepath : { "test_image.png" })
    {
        // Create a dummy image asset under Assets (.png extension maps to AssetType::Texture)
        ignite::Path assetFilepath = project->GetAssetDirectory() / texFilepath;
        {
            std::ofstream file(assetFilepath.generic_string());
            file << "Fake PNG Data";
            file.close();
        }

        // Import the asset
        AssetHandle handle = assetManager->ImportAsset(assetFilepath);
        EXPECT_NE(handle, AssetHandle(0));

        // Verify asset is registered and has metadata
        EXPECT_TRUE(assetManager->GetAssetAssetRegistry().contains(handle));
        AssetMetaData meta = assetManager->GetMetaData(handle);
        EXPECT_EQ(meta.type, AssetType::Texture);
        EXPECT_EQ(project->GetProjectRelativeFilepath(meta.filepath).generic_string(), std::format("Assets/{}", texFilepath));
    }
    
}

// -------------------------------------------------
// Project Serialization & Deserialization Test
// -------------------------------------------------
TEST(EngineTests, Serialization)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/SerializationProject";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "SerializationProject";
    info.filepath = projectDir / "SerializationProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Release;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);
    
    // Set default scene handle and serialize
    AssetHandle testSceneHandle(42);
    project->SetDefaultScene(testSceneHandle);
    ASSERT_TRUE(project->Serialize(info.filepath));

    // Deserialize the project from file
    Ref<Project> deserializedProject = Project::Deserialize(info.filepath);
    ASSERT_NE(deserializedProject, nullptr);

    // Verify serialized properties match
    EXPECT_EQ(deserializedProject->GetInfo().name, project->GetInfo().name);
    EXPECT_EQ(deserializedProject->GetInfo().defaultSceneHandle, testSceneHandle);
    EXPECT_EQ(deserializedProject->GetConfiguration(), ProjectConfiguration::Release);
}

// -------------------------------------------------
// C# Scripting Test
// -------------------------------------------------
TEST(EngineTests, CSharpScripting)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/ScriptingProject";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "ScriptingProject";
    info.filepath = projectDir / "ScriptingProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    // Initialize the C# script engine
    project->InitScriptEngine();

    // Create a new C# script
    ignite::Path scriptFilepath = project->GetScriptsDirectory() / "Player.cs";
    project->CreateCSharpScript(scriptFilepath);
    EXPECT_TRUE(ignite::Path::exists(scriptFilepath));

    // Wait for compilation & script engine loading assembly
    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);

    auto startTime = std::chrono::steady_clock::now();
    while (!scriptEngine->IsReady())
    {
        Application::GetInstance()->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Timeout after 30 seconds
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
        {
            break;
        }
    }

    EXPECT_TRUE(scriptEngine->IsReady());
}

// -------------------------------------------------
// Input System & Action Mapping Test
// -------------------------------------------------
TEST(EngineTests, ActionInputSystem)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/ActionInputProject";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "ActionInputProject";
    info.filepath = projectDir / "ActionInputProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    // Initialize the C# script engine
    project->InitScriptEngine();

    // Create a new C# script ActionTest.cs
    ignite::Path scriptFilepath = project->GetScriptsDirectory() / "ActionTest.cs";
    {
        std::ofstream out(scriptFilepath.generic_string());
        out << R"(using Ignite;
using System;

namespace ActionInputProject;

public class ActionTest : Entity
{
    public override void OnCreate()
    {
        bool isJumpPressed = Input.IsActionPressed("Jump");
        bool isFirePressed = Input.IsActionPressed("Fire");
        Debug.Log("ActionTest C# OnCreate triggered");
        Debug.Log("Jump pressed: " + isJumpPressed);
        Debug.Log("Fire pressed: " + isFirePressed);
    }
}
)";
        out.close();
        project->RegenerateCSharpProject();
    }

    // Wait for compilation & script engine loading assembly
    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);

    auto startTime = std::chrono::steady_clock::now();
    while (!scriptEngine->IsReady())
    {
        Application::GetInstance()->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
        {
            break;
        }
    }
    ASSERT_TRUE(scriptEngine->IsReady());

    // Create and register an InputMapping
    Ref<InputMapping> mapping = CreateRef<InputMapping>();
    mapping->MapKey(Key::Space, "Jump");
    mapping->MapKey(Key::F, "Fire");
    
    InputSystem::SetInputMapping(mapping);

    // Verify initial C++ side mapping state
    EXPECT_FALSE(InputSystem::IsActionPressed("Jump"));
    EXPECT_FALSE(InputSystem::IsActionPressed("Fire"));

    // Simulate Key Press
    InputSystem::GetActiveSystem()->SetKey(Key::Space, true);
    EXPECT_TRUE(InputSystem::IsActionPressed("Jump"));
    EXPECT_FALSE(InputSystem::IsActionPressed("Fire"));

    // Run C# side invocation
    auto scriptInstance = scriptEngine->OnCreateEntityInstance(101, "ActionInputProject.ActionTest");
    ASSERT_NE(scriptInstance, nullptr);
    scriptInstance->InvokeOnCreate();

    // Release Key
    InputSystem::GetActiveSystem()->SetKey(Key::Space, false);
    EXPECT_FALSE(InputSystem::IsActionPressed("Jump"));
}

// -------------------------------------------------
// Animator C# Scripting Integration Test
// -------------------------------------------------
TEST(EngineTests, AnimatorScriptingIntegration)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/AnimatorScriptProject";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "AnimatorScriptProject";
    info.filepath = projectDir / "AnimatorScriptProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    project->InitScriptEngine();

    // Create C# script AnimationTest.cs
    ignite::Path scriptFilepath = project->GetScriptsDirectory() / "AnimationTest.cs";
    {
        std::ofstream out(scriptFilepath.generic_string());
        out << R"(using Ignite;
using System;

namespace AnimatorScriptProject;

public class AnimationTest : Entity
{
    private AnimatorComponent _animator;

    public override void OnCreate()
    {
        _animator = AddComponent<AnimatorComponent>();
    }

    public override void OnUpdate(float deltaTime)
    {
        if (_animator != null)
        {
            _animator.SetFloat("Speed", 5.0f);
            _animator.SetFloat("Direction", -90.0f);
            _animator.SetBool("IsMoving", true);
            _animator.SetInt("StateIndex", 2);
            _animator.SetString("StateName", "Running");
            _animator.SetState("WalkRunState");
        }
    }
}
)";
        out.close();
        project->RegenerateCSharpProject();
    }

    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);

    auto startTime = std::chrono::steady_clock::now();
    while (!scriptEngine->IsReady())
    {
        Application::GetInstance()->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
        {
            break;
        }
    }
    ASSERT_TRUE(scriptEngine->IsReady());

    Ref<Scene> scene = Scene::Create(project.get(), "TestScene");
    scriptEngine->SetSceneContext(scene.get());

    Entity entity = SceneManager::CreateEntity(scene.get(), "TestPlayer", EntityType_Node);
    uint64_t entityID = static_cast<uint64_t>(entity.GetUUID());

    auto &meshComp = entity.AddComponent<SkeletalMeshComponent>();
    Ref<AnimatorController> controller = AnimatorController::Create();
    controller->params = {
        { .name = "Speed", .floatVal = 0.0f, .type = AnimParam::Type::Float },
        { .name = "Direction", .floatVal = 0.0f, .type = AnimParam::Type::Float },
        { .name = "IsMoving", .boolVal = false, .type = AnimParam::Type::Bool },
        { .name = "StateIndex", .intVal = 0, .type = AnimParam::Type::Int },
        { .name = "StateName", .strVal = "", .type = AnimParam::Type::String }
    };

    AnimState blendState;
    blendState.name = "WalkRunState";
    blendState.SetMotion(AnimState::MotionType::BlendSpace, AssetHandle(888));
    controller->states.push_back(blendState);
    controller->defaultState = "WalkRunState";

    meshComp.runtimeAnimatorInstance = controller;
    meshComp.runtimeParams = controller->params;

    auto scriptInstance = scriptEngine->OnCreateEntityInstance(entityID, "AnimatorScriptProject.AnimationTest");
    ASSERT_NE(scriptInstance, nullptr);

    scriptInstance->InvokeOnUpdate(0.016f);

    const AnimParam *speed = controller->GetParam("Speed");
    const AnimParam *direction = controller->GetParam("Direction");
    const AnimParam *isMoving = controller->GetParam("IsMoving");
    const AnimParam *stateIndex = controller->GetParam("StateIndex");
    const AnimParam *stateName = controller->GetParam("StateName");

    ASSERT_NE(speed, nullptr);
    ASSERT_NE(direction, nullptr);
    ASSERT_NE(isMoving, nullptr);
    ASSERT_NE(stateIndex, nullptr);
    ASSERT_NE(stateName, nullptr);

    EXPECT_FLOAT_EQ(speed->floatVal, 5.0f);
    EXPECT_FLOAT_EQ(direction->floatVal, -90.0f);
    EXPECT_TRUE(isMoving->boolVal);
    EXPECT_EQ(stateIndex->intVal, 2);
    EXPECT_EQ(stateName->strVal, "Running");
}

// -------------------------------------------------
// Scene Transition Test Suite
// -------------------------------------------------
TEST(SceneTransition, BasicTransition)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/SceneTransitionBasic";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "SceneTransitionBasicProject";
    info.filepath = projectDir / "SceneTransitionBasicProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    project->InitScriptEngine();
    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);
    auto startTime = std::chrono::steady_clock::now();
    while (!scriptEngine->IsReady())
    {
        Application::GetInstance()->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
        {
            break;
        }
    }
    ASSERT_TRUE(scriptEngine->IsReady());

    AssetManager *assetManager = AssetManager::GetInstance();
    ASSERT_NE(assetManager, nullptr);
	assetManager->SetActiveProject(project);

    Ref<Scene> sceneA = Scene::Create(project.get(), "SceneA");
    Ref<Scene> sceneB = Scene::Create(project.get(), "SceneB");

    AssetHandle sceneAHandle(101);
    AssetHandle sceneBHandle(102);

    sceneA->handle = sceneAHandle;
    sceneB->handle = sceneBHandle;

    AssetMetaData metaA;
    metaA.type = AssetType::Scene;
    metaA.filepath = "Assets/SceneA.ixscene";
    assetManager->AssignMetaData(sceneAHandle, metaA);
    assetManager->AssignAsset(sceneAHandle, sceneA);

    AssetMetaData metaB;
    metaB.type = AssetType::Scene;
    metaB.filepath = "Assets/SceneB.ixscene";
    assetManager->AssignMetaData(sceneBHandle, metaB);
    assetManager->AssignAsset(sceneBHandle, sceneB);

    project->SetActiveScene(sceneA);
    sceneA->OnStart(ESceneState::Play);

    {
        auto activeScene = project->LockActiveScene();
        ASSERT_NE(activeScene, nullptr);
        LOG_INFO("DEBUG basic: active scene at start: {}, running state: {}", (void *)activeScene.get(), (int)sceneA->GetState());
        EXPECT_EQ(activeScene, sceneA);
    }

    SceneManager::Transition(sceneBHandle);
    Application::GetInstance()->ProcessMainThreadSubmissions();
    LOG_INFO("DEBUG basic: calling ExecutePendingTransition");
    SceneManager::ExecutePendingTransition();

    {
        auto activeScene = project->LockActiveScene();
        ASSERT_NE(activeScene, nullptr);
        LOG_INFO("DEBUG basic: active scene at end: {}, running state sceneA: {}, running state activeScene: {}", (void *)activeScene.get(), (int)sceneA->GetState(), (int)activeScene->GetState());

        EXPECT_NE(activeScene, sceneA);
        EXPECT_EQ(activeScene->name, "SceneB");
        EXPECT_TRUE(activeScene->IsRunning());
        EXPECT_FALSE(sceneA->IsRunning());
    }
}

TEST(SceneTransition, SharedAssetPinned)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/SceneTransitionShared";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "SceneTransitionSharedProject";
    info.filepath = projectDir / "SceneTransitionSharedProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    project->InitScriptEngine();
    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);
    auto startTime = std::chrono::steady_clock::now();
    while (!scriptEngine->IsReady())
    {
        Application::GetInstance()->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
        {
            break;
        }
    }
    ASSERT_TRUE(scriptEngine->IsReady());

    AssetManager *assetManager = AssetManager::GetInstance();
    ASSERT_NE(assetManager, nullptr);
	assetManager->SetActiveProject(project);

    Ref<Scene> sceneA = Scene::Create(project.get(), "SceneA");
    Ref<Scene> sceneB = Scene::Create(project.get(), "SceneB");

    AssetHandle sceneAHandle(201);
    AssetHandle sceneBHandle(202);

    sceneA->handle = sceneAHandle;
    sceneB->handle = sceneBHandle;

    AssetMetaData metaA;
    metaA.type = AssetType::Scene;
    metaA.filepath = "Assets/SceneA.ixscene";
    assetManager->AssignMetaData(sceneAHandle, metaA);
    assetManager->AssignAsset(sceneAHandle, sceneA);

    AssetMetaData metaB;
    metaB.type = AssetType::Scene;
    metaB.filepath = "Assets/SceneB.ixscene";
    assetManager->AssignMetaData(sceneBHandle, metaB);
    assetManager->AssignAsset(sceneBHandle, sceneB);

    AssetHandle sharedTextureHandle(301);
    AssetMetaData textureMeta;
    textureMeta.type = AssetType::Texture;
    textureMeta.filepath = "Assets/SharedTexture.png";
    assetManager->AssignMetaData(sharedTextureHandle, textureMeta);
    
    Ref<Asset> dummyTexture = CreateRef<Asset>();
    dummyTexture->SetReadyFlag(true);
    assetManager->AssignAsset(sharedTextureHandle, dummyTexture);

    auto entityB = sceneB->registry->create();
    Sprite2DComponent sprite;
    sprite.handle = sharedTextureHandle;
    sceneB->registry->emplace<Sprite2DComponent>(entityB, sprite);

    project->SetActiveScene(sceneA);
    sceneA->OnStart(ESceneState::Play);

    EXPECT_TRUE(assetManager->IsAssetLoaded(sharedTextureHandle));

    SceneManager::Transition(sceneBHandle);
    Application::GetInstance()->ProcessMainThreadSubmissions();
    SceneManager::ExecutePendingTransition();

    assetManager->UnloadUnusedAssets();

    EXPECT_TRUE(assetManager->IsAssetLoaded(sharedTextureHandle));
}

TEST(SceneTransition, InvalidHandleRejected)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/SceneTransitionInvalid";

    if (ignite::Path::exists(projectDir))
    {
        std::filesystem::remove_all(projectDir.string());
    }
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "SceneTransitionInvalidProject";
    info.filepath = projectDir / "SceneTransitionInvalidProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    project->InitScriptEngine();
    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);
    auto startTime = std::chrono::steady_clock::now();
    while (!scriptEngine->IsReady())
    {
        Application::GetInstance()->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(30))
        {
            break;
        }
    }
    ASSERT_TRUE(scriptEngine->IsReady());

    LOG_INFO("DEBUG: Creating sceneA");
    Ref<Scene> sceneA = Scene::Create(project.get(), "SceneA");
    LOG_INFO("DEBUG: Setting active scene to A");
    project->SetActiveScene(sceneA);

    LOG_INFO("DEBUG: TransitionTo 0");
    SceneManager::Transition(AssetHandle(0));
    LOG_INFO("DEBUG: ExecutePendingTransition 0");
    SceneManager::ExecutePendingTransition();

    LOG_INFO("DEBUG: Checking active scene is still A");
	EXPECT_EQ(project->LockActiveScene(), sceneA);

    LOG_INFO("DEBUG: TransitionTo 9999");
    SceneManager::Transition(AssetHandle(9999));
    LOG_INFO("DEBUG: ExecutePendingTransition 9999");
    SceneManager::ExecutePendingTransition();

    LOG_INFO("DEBUG: Checking active scene is still A (final)");
    EXPECT_EQ(project->LockActiveScene(), sceneA);
    LOG_INFO("DEBUG: Reached end of test body");
}

// -------------------------------------------------
// Hot-Reload Field Reflection Test
// -------------------------------------------------
TEST(EngineTests, ScriptHotReload)
{
    ignite::Path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
    ignite::Path projectDir = testResourcesRoot / "temp/ScriptHotReloadProject";

    if (ignite::Path::exists(projectDir))
        std::filesystem::remove_all(projectDir.string());
    ignite::Path::create_directories(projectDir);

    ProjectInfo info;
    info.name = "ScriptHotReloadProject";
    info.filepath = projectDir / "ScriptHotReloadProject.ixproj";
    info.rootDirectory = projectDir;
    info.assetDirectory = "Assets";
    info.scriptsDirectory = "Scripts";
    info.assetRegistryFilepath = "AssetRegistry.ixreg";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    // --- Write initial Player.cs (one public field) ---
    ignite::Path playerScript = project->GetScriptsDirectory() / "Player.cs";
    {
        std::ofstream out(playerScript.generic_string());
        out << R"(using Ignite;
namespace ScriptHotReloadProject;
public class Player : Entity
{
    public float Speed;
}
)";
    }

    // --- Write initial GameSettings.cs (ScriptableObject, one public field) ---
    ignite::Path settingsScript = project->GetScriptsDirectory() / "GameSettings.cs";
    {
        std::ofstream out(settingsScript.generic_string());
        out << R"(using Ignite;
namespace ScriptHotReloadProject;
[CreateAssetMenu(FileName = "GameSettings", MenuName = "Settings/Game")]
public class GameSettings : ScriptableObject
{
    public int MaxPlayers;
}
)";
    }

    project->RegenerateCSharpProject();
    project->InitScriptEngine();

    auto scriptEngine = project->GetScriptEngine();
    ASSERT_NE(scriptEngine, nullptr);

    // --- Wait for initial load ---
    auto waitReady = [&](int timeoutSecs) {
        auto start = std::chrono::steady_clock::now();
        while (!scriptEngine->IsReady())
        {
            Application::GetInstance()->ProcessMainThreadSubmissions();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(timeoutSecs))
                break;
        }
    };
    waitReady(30);
    ASSERT_TRUE(scriptEngine->IsReady());

    // Verify initial entity class & field
    EXPECT_TRUE(scriptEngine->IsEntityClassExists("ScriptHotReloadProject.Player"));
    {
        auto cls = scriptEngine->GetEntityClassByName("ScriptHotReloadProject.Player");
        ASSERT_NE(cls, nullptr);
        auto &fields = cls->GetFields();
        EXPECT_TRUE(fields.count("Speed") > 0);
        if (fields.count("Speed"))
            EXPECT_EQ(fields.at("Speed").Type, ScriptFieldType::Float);
    }

    // Verify initial ScriptableObject class
    EXPECT_TRUE(scriptEngine->IsScriptableObjectClassExists("ScriptHotReloadProject.GameSettings"));
    {
        auto cls = scriptEngine->GetScriptableObjectClassByName("ScriptHotReloadProject.GameSettings");
        ASSERT_NE(cls, nullptr);
        EXPECT_TRUE(cls->GetFields().count("MaxPlayers") > 0);
    }

    // --- Modify Player.cs: add [SerializeField] private int Health ---
    {
        std::ofstream out(playerScript.generic_string());
        out << R"(using Ignite;
namespace ScriptHotReloadProject;
public class Player : Entity
{
    public float Speed;
    [SerializeField] private int Health;
}
)";
    }

    // --- Modify GameSettings.cs: add a new field ---
    {
        std::ofstream out(settingsScript.generic_string());
        out << R"(using Ignite;
namespace ScriptHotReloadProject;
[CreateAssetMenu(FileName = "GameSettings", MenuName = "Settings/Game")]
public class GameSettings : ScriptableObject
{
    public int MaxPlayers;
    public float RoundTime;
}
)";
    }

    // Trigger recompilation and hot-reload
    project->RegenerateCSharpProject();
    project->BuildSolution(true);

    // Wait for engine to go not-ready then become ready again (hot-reload cycle)
    // Give it a moment to detect the change via file-watcher
    {
        auto start = std::chrono::steady_clock::now();
        while (scriptEngine->IsReady()
               && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
        {
            Application::GetInstance()->ProcessMainThreadSubmissions();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    waitReady(30);
    ASSERT_TRUE(scriptEngine->IsReady());

    // --- Verify reloaded entity class has both fields ---
    EXPECT_TRUE(scriptEngine->IsEntityClassExists("ScriptHotReloadProject.Player"));
    {
        auto cls = scriptEngine->GetEntityClassByName("ScriptHotReloadProject.Player");
        ASSERT_NE(cls, nullptr);
        auto &fields = cls->GetFields();

        // Speed (public) must still be present
        EXPECT_TRUE(fields.count("Speed") > 0) << "Speed field missing after reload";
        if (fields.count("Speed"))
            EXPECT_EQ(fields.at("Speed").Type, ScriptFieldType::Float);

        // Health ([SerializeField]) must now appear
        EXPECT_TRUE(fields.count("Health") > 0) << "Health field missing after reload";
        if (fields.count("Health"))
        {
            EXPECT_EQ(fields.at("Health").Type, ScriptFieldType::Int);
            EXPECT_TRUE(fields.at("Health").HasSerializeFieldAttribute);
            EXPECT_FALSE(fields.at("Health").IsPublic);
        }
    }

    // --- Verify reloaded ScriptableObject class has both fields ---
    EXPECT_TRUE(scriptEngine->IsScriptableObjectClassExists("ScriptHotReloadProject.GameSettings"));
    {
        auto cls = scriptEngine->GetScriptableObjectClassByName("ScriptHotReloadProject.GameSettings");
        ASSERT_NE(cls, nullptr);
        auto &fields = cls->GetFields();
        EXPECT_TRUE(fields.count("MaxPlayers") > 0) << "MaxPlayers missing after SO reload";
        EXPECT_TRUE(fields.count("RoundTime") > 0) << "RoundTime missing after SO reload";
    }
}

int main(int argc, char **argv)
{
    ignite::Logger::Init();

    ignite::ApplicationCreateInfo createInfo{};
    createInfo.cmdLineArgs = { argc, argv };
    createInfo.name = "Ignite Test Headless";
    createInfo.useGui = false;
    createInfo.usePhysics = true;
    createInfo.useAudio = false;
    createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
    createInfo.headless = true;

    auto app = CreateScope<ignite::Application>(createInfo);

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    app.reset();
    ignite::Logger::Shutdown();

    exit(result);
}
