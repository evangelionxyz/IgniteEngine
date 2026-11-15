// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/agent_controller.hpp"
#include "ignite/project/game_packager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"

#include <filesystem>
#include <fstream>

using namespace ignite;

TEST(AgentAndPackager, AgentCommandsBuildSimulateRaycastAndSerializeScene)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteAgentFeatureTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir / "Assets" / "Scenes", ec);

    ProjectInfo info;
    info.name = "AgentTestGame";
    info.rootDirectory = tempDir;
    info.filepath = tempDir / "AgentTestGame.ixproj";
    Ref<Project> project = Project::Create(info);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    AgentExecutionContext ctx;
    ctx.scene = scene.get();
    ctx.project = project.get();
    ctx.onSceneReplaced = [&](const Ref<Scene> &s)
    {
        scene = s;
        ctx.scene = scene.get();
        project->SetActiveScene(s);
    };

    // 1. Ping status
    {
        const auto res = AgentController::ExecuteCommand({ { "command", "ping" } }, ctx);
        ASSERT_TRUE(res.value("ok", false));
        EXPECT_TRUE(res["data"].value("has_scene", false));
        EXPECT_EQ(res["data"].value("project_name", ""), "AgentTestGame");
    }

    // 2. Create floor plane with static physics collider + hero cube
    {
        const auto floorRes = AgentController::ExecuteCommand({
            { "command", "create_entity" },
            { "name", "GroundPlane" },
            { "primitive", "Plane" },
            { "position", { 0.0f, 0.0f, 0.0f } },
            { "scale", { 2.0f, 1.0f, 2.0f } }
        }, ctx);
        ASSERT_TRUE(floorRes.value("ok", false)) << floorRes.dump();

        ASSERT_TRUE(AgentController::ExecuteCommand({
            { "command", "add_component" },
            { "entity", "GroundPlane" },
            { "type", "Rigidbody" },
            { "properties", { { "body_type", "static" } } }
        }, ctx).value("ok", false));

        ASSERT_TRUE(AgentController::ExecuteCommand({
            { "command", "add_component" },
            { "entity", "GroundPlane" },
            { "type", "BoxCollider" },
            { "properties", { { "size", { 20.0f, 0.5f, 20.0f } }, { "center", { 0.0f, -0.25f, 0.0f } } } }
        }, ctx).value("ok", false));

        const auto cubeRes = AgentController::ExecuteCommand({
            { "command", "create_entity" },
            { "name", "PlayerCube" },
            { "primitive", "Cube" },
            { "position", { 0.0f, 3.0f, 0.0f } }
        }, ctx);
        ASSERT_TRUE(cubeRes.value("ok", false));

        ASSERT_TRUE(AgentController::ExecuteCommand({
            { "command", "add_component" },
            { "entity", "PlayerCube" },
            { "type", "PointLight" },
            { "properties", { { "color", { 1.0f, 0.7f, 0.3f } }, { "intensity", 4.0f }, { "radius", 12.0f } } }
        }, ctx).value("ok", false));

        ASSERT_TRUE(AgentController::ExecuteCommand({
            { "command", "add_component" },
            { "entity", "PlayerCube" },
            { "type", "Camera" },
            { "properties", { { "primary", true }, { "fov", 65.0f } } }
        }, ctx).value("ok", false));
    }

    // 3. Duplicate, rename, destroy entity check
    {
        const auto dup = AgentController::ExecuteCommand({
            { "command", "duplicate_entity" },
            { "entity", "PlayerCube" }
        }, ctx);
        ASSERT_TRUE(dup.value("ok", false));
        const uint64_t dupId = dup["data"].value("id", uint64_t{ 0 });
        ASSERT_NE(dupId, 0ULL);

        ASSERT_TRUE(AgentController::ExecuteCommand({
            { "command", "rename_entity" },
            { "entity", dupId },
            { "new_name", "TempClone" }
        }, ctx).value("ok", false));

        ASSERT_TRUE(AgentController::ExecuteCommand({
            { "command", "destroy_entity" },
            { "entity", "TempClone" }
        }, ctx).value("ok", false));
    }

    // 4. Start simulation via AgentController, step frames, test raycast + run_lua
    scene->OnStart(ESceneState::Simulate);
    {
        const auto stepRes = AgentController::ExecuteCommand({
            { "command", "step_frames" },
            { "frames", 5 },
            { "dt", 1.0f / 60.0f }
        }, ctx);
        EXPECT_TRUE(stepRes.value("ok", false));

        const auto rayRes = AgentController::ExecuteCommand({
            { "command", "physics_raycast" },
            { "origin", { 0.0f, 5.0f, 0.0f } },
            { "direction", { 0.0f, -1.0f, 0.0f } },
            { "max_distance", 50.0f }
        }, ctx);
        ASSERT_TRUE(rayRes.value("ok", false));
        EXPECT_TRUE(rayRes["data"].value("hit", false)) << rayRes.dump();
        EXPECT_EQ(rayRes["data"].value("entity_name", ""), "GroundPlane");

        const auto luaRes = AgentController::ExecuteCommand({
            { "command", "run_lua" },
            { "code", "local c = Ignite.FindEntityByName('PlayerCube'); return c and c:get_component('Transform').position.y or -1" }
        }, ctx);
        ASSERT_TRUE(luaRes.value("ok", false)) << luaRes.dump();
        EXPECT_NEAR(std::stof(luaRes["data"].value("result", "0")), 3.0f, 0.01f);
    }
    scene->OnStop();

    // 5. Save and reload scene roundtrip
    const std::filesystem::path sceneFile = tempDir / "Assets" / "Scenes" / "Arena.ixscene";
    {
        const auto saveRes = AgentController::ExecuteCommand({
            { "command", "save_scene" },
            { "path", sceneFile.string() }
        }, ctx);
        ASSERT_TRUE(saveRes.value("ok", false));
        EXPECT_TRUE(std::filesystem::exists(sceneFile));

        const auto loadRes = AgentController::ExecuteCommand({
            { "command", "load_scene" },
            { "path", sceneFile.string() }
        }, ctx);
        ASSERT_TRUE(loadRes.value("ok", false));
        auto findByName = [&](std::string_view n) -> Entity
        {
            auto view = scene->registry->view<IDComponent>();
            for (entt::entity e : view)
            {
                if (view.get<IDComponent>(e).name == n)
                    return Entity{ e, scene.get() };
            }
            return {};
        };
        EXPECT_TRUE(static_cast<bool>(findByName("GroundPlane")));
        EXPECT_TRUE(static_cast<bool>(findByName("PlayerCube")));
    }

    std::filesystem::remove_all(tempDir, ec);
}

TEST(AgentAndPackager, PackageProjectExportsSelfContainedDirectory)
{
    EXPECT_EQ(GamePackager::SanitizeExecutableName("My Cool Game!"), "My_Cool_Game");

    const std::filesystem::path runtimeBinDir = GamePackager::FindRuntimeBinaryDirectory();
    if (runtimeBinDir.empty() || !std::filesystem::exists(runtimeBinDir / "Ignite.Runtime.exe"))
    {
        GTEST_SKIP() << "Ignite.Runtime.exe not built yet in runtime binary directory";
    }

    const std::filesystem::path baseDir = std::filesystem::temp_directory_path() / "IgnitePackagerUnitTest";
    const std::filesystem::path projDir = baseDir / "SampleGame";
    const std::filesystem::path outDir = baseDir / "Dist";

    std::error_code ec;
    std::filesystem::remove_all(baseDir, ec);
    std::filesystem::create_directories(projDir / "Assets" / "Scenes", ec);
    std::filesystem::create_directories(projDir / "Scripts", ec);

    {
        std::ofstream(projDir / "Assets" / "Scenes" / "Start.ixscene") << "Scene: Start\n";
        std::ofstream(projDir / "Scripts" / "main.lua") << "return {}\n";
    }

    ProjectInfo info;
    info.name = "My Cool Game!";
    info.rootDirectory = projDir;
    info.filepath = projDir / "SampleGame.ixproj";
    Ref<Project> project = Project::Create(info);
    project->Serialize(info.filepath);

    PackageBuildConfig cfg;
    cfg.outputDirectory = outDir;
    cfg.runtimeBinaryDirectory = runtimeBinDir;
    cfg.includeDedicatedServer = true;

    const PackageBuildResult result = GamePackager::PackageProject(project.get(), cfg);
    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_TRUE(std::filesystem::exists(result.executablePath));
    EXPECT_EQ(result.executablePath.filename().string(), "My_Cool_Game.exe");
    EXPECT_TRUE(std::filesystem::exists(outDir / "SampleGame.ixproj"));
    EXPECT_TRUE(std::filesystem::exists(outDir / "Assets" / "Scenes" / "Start.ixscene"));
    EXPECT_TRUE(std::filesystem::exists(outDir / "Scripts" / "main.lua"));
    EXPECT_FALSE(std::filesystem::exists(outDir / "Ignite.Editor.exe"));

    std::filesystem::remove_all(baseDir, ec);
}
