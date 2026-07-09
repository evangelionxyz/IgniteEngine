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
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <format>

#include <gtest/gtest.h>

using namespace ignite;

// -------------------------------------------------
// Serializer and Deserializer test for InputMapping
// -------------------------------------------------

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

    AssetManager *assetManager = project->GetAssetManager();
    ASSERT_NE(assetManager, nullptr);

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

int main(int argc, char **argv)
{
    ignite::Logger::Init();

    ignite::ApplicationCreateInfo createInfo{};
    createInfo.cmdLineArgs = { argc, argv };
    createInfo.name = "Ignite Test Headless";
    createInfo.useGui = false;
    createInfo.usePhysics = false;
    createInfo.useAudio = false;
    createInfo.graphicsApi = nvrhi::GraphicsAPI::VULKAN;
    createInfo.headless = true;

    auto app = CreateScope<ignite::Application>(createInfo);

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    app.reset();
    ignite::Logger::Shutdown();

    return result;
}
