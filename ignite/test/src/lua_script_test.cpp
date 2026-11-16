// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/scripting/lua/lua_script_runtime.hpp"
#include "ignite/scripting/lua/lua_script.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/asset/asset_types.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/scene_serializer.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/skeleton.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <format>

using namespace ignite;

namespace
{
    std::filesystem::path CreateTempScript(std::string_view prefix, std::string_view content)
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path path = std::filesystem::temp_directory_path() / std::format("{}_{}.lua", prefix, unique);
        std::ofstream script(path);
        script << content;
        return path;
    }
}

TEST(LuaScriptRuntime, LifecycleAndIsolatedEnvironments)
{
    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_lifecycle", R"lua(
        local script = {}
        function script.on_create(entity)
            local transform = entity:get_component("Transform")
            transform.position = { x = 0, y = 2, z = 0 }
        end
        function script.on_update(entity, dt)
            counter = (counter or 0) + 1
            local transform = entity:get_component("Transform")
            transform.position = { x = counter, y = 2, z = 0 }
        end
        function script.on_fixed_update(entity)
            local transform = entity:get_component("Transform")
            transform.position = { x = transform.position.x, y = 2, z = 3 }
        end
        function script.on_destroy(entity)
            local transform = entity:get_component("Transform")
            transform.scale = { x = 4, y = 1, z = 1 }
        end
        return script
    )lua");

    const std::filesystem::path errorScriptPath = CreateTempScript("ignite_lua_err", "return { on_update = function() error('expected error') end }");
    const std::filesystem::path syntaxErrorPath = CreateTempScript("ignite_lua_syn", "return { on_update = function( end");

    Scene scene;
    Entity first = SceneManager::CreateEntity(&scene, "LuaFirst", EntityType_Node);
    Entity second = SceneManager::CreateEntity(&scene, "LuaSecond", EntityType_Node);
    Entity failing = SceneManager::CreateEntity(&scene, "LuaFailing", EntityType_Node);

    first.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;
    second.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;
    failing.AddComponent<LuaScriptComponent>().scriptPath = errorScriptPath;

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.CreateInstance(first.GetUUID(), scriptPath);
        runtime.CreateInstance(second.GetUUID(), scriptPath);
        runtime.CreateInstance(failing.GetUUID(), errorScriptPath);
        runtime.CreateInstance(ignite::UUID(static_cast<uint64_t>(-1)), scriptPath.parent_path() / "missing_lua_script.lua");
        runtime.CreateInstance(ignite::UUID(static_cast<uint64_t>(-2)), syntaxErrorPath);

        EXPECT_FLOAT_EQ(first.GetTransform().local.translation.y, 2.0f);
        runtime.Update(1.0f / 60.0f);
        EXPECT_FLOAT_EQ(first.GetTransform().local.translation.x, 1.0f);
        EXPECT_FLOAT_EQ(second.GetTransform().local.translation.x, 1.0f);

        runtime.Update(1.0f / 60.0f);
        EXPECT_FLOAT_EQ(first.GetTransform().local.translation.x, 2.0f);
        EXPECT_FLOAT_EQ(second.GetTransform().local.translation.x, 2.0f);

        runtime.FixedUpdate();
        EXPECT_FLOAT_EQ(first.GetTransform().local.translation.z, 3.0f);

        runtime.DestroyInstance(first.GetUUID());
        EXPECT_FLOAT_EQ(first.GetTransform().local.scale.x, 4.0f);
    }

    std::filesystem::remove(scriptPath);
    std::filesystem::remove(errorScriptPath);
    std::filesystem::remove(syntaxErrorPath);
}

TEST(LuaScriptRuntime, ComponentAccessAndMutations)
{
    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_comps", R"lua(
        local script = {}
        function script.on_create(entity)
            local transform = entity:get_component("Transform")
            transform.position = { x = 5, y = 10, z = 15 }
            transform.rotation = { x = 45, y = 0, z = 90 }
            transform.scale = { x = 2, y = 2, z = 2 }

            local rb2d = entity:get_component("Rigidbody2D")
            rb2d.linear_velocity = { x = 3.5, y = -1.5, z = 0 }
            rb2d.angular_velocity = 2.0
            rb2d.gravity_scale = 0.5
            rb2d:apply_force(10.0, 20.0)

            local sprite = entity:get_component("Sprite2D")
            sprite.flip_x = true
            sprite.flip_y = false
            sprite.color = { 1.0, 0.5, 0.25, 1.0 }

            local audio = entity:get_component("AudioSource")
            audio.volume = 0.8
            audio.pitch = 1.2
            audio.pan = -0.5
            audio.loop = true
            audio.play_on_start = true
        end
        return script
    )lua");

    Scene scene;
    Entity entity = SceneManager::CreateEntity(&scene, "TestComps", EntityType_Node);
    entity.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;
    entity.AddComponent<Rigidbody2DComponent>();
    entity.AddComponent<Sprite2DComponent>();
    entity.AddComponent<AudioSourceComponent>();

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.CreateInstance(entity.GetUUID(), scriptPath);

        // Verify Transform
        const auto &t = entity.GetComponent<TransformComponent>().local;
        EXPECT_NEAR(t.translation.x, 5.0f, 1e-4f);
        EXPECT_NEAR(t.translation.y, 10.0f, 1e-4f);
        EXPECT_NEAR(t.translation.z, 15.0f, 1e-4f);
        EXPECT_NEAR(t.scale.x, 2.0f, 1e-4f);

        // Verify Rigidbody2D
        const auto &rb = entity.GetComponent<Rigidbody2DComponent>();
        // linear_velocity was (3.5, -1.5) and apply_force added (10, 20) without Box2D world
        EXPECT_NEAR(rb.linearVelocity.x, 13.5f, 1e-4f);
        EXPECT_NEAR(rb.linearVelocity.y, 18.5f, 1e-4f);
        EXPECT_FLOAT_EQ(rb.angularVelocity, 2.0f);
        EXPECT_FLOAT_EQ(rb.gravityScale, 0.5f);

        // Verify Sprite2D
        const auto &sp = entity.GetComponent<Sprite2DComponent>();
        EXPECT_TRUE(sp.flipX);
        EXPECT_FALSE(sp.flipY);
        EXPECT_FLOAT_EQ(sp.color.r, 1.0f);
        EXPECT_FLOAT_EQ(sp.color.g, 0.5f);
        EXPECT_FLOAT_EQ(sp.color.b, 0.25f);
        EXPECT_FLOAT_EQ(sp.color.a, 1.0f);

        // Verify AudioSource
        const auto &as = entity.GetComponent<AudioSourceComponent>();
        EXPECT_FLOAT_EQ(as.volume, 0.8f);
        EXPECT_FLOAT_EQ(as.pitch, 1.2f);
        EXPECT_FLOAT_EQ(as.pan, -0.5f);
        EXPECT_TRUE(as.loop);
        EXPECT_TRUE(as.playOnStart);
    }

    std::filesystem::remove(scriptPath);
}

TEST(LuaScriptRuntime, NetworkInputCallbackReceivesOwnedEntityAndInput)
{
    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_network_input", R"lua(
        local script = {}
        function script.on_network_client_joined(entity, client_id, client_name)
            entity:get_component("Transform").position = { x = client_id, y = 0, z = 0 }
        end
        function script.on_network_input(entity, client_id, move_x, move_y, buttons, yaw, dt)
            local transform = entity:get_component("Transform")
            transform.position = {
                x = move_x + client_id,
                y = buttons,
                z = move_y + yaw + dt
            }
        end
        return script
    )lua");

    Scene scene;
    Entity player = SceneManager::CreateEntity(&scene, "NetworkPlayer", EntityType_Node);
    player.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.InvokeNetworkEvent("on_network_client_joined", 7, "ClientSeven");
        EXPECT_FLOAT_EQ(player.GetTransform().local.translation.x, 7.0f);
        runtime.InvokeNetworkInput(player.GetUUID(), 3, 0.25f, -0.5f, 2, 45.0f, 1.0f / 60.0f);

        const glm::vec3 position = player.GetTransform().local.translation;
        EXPECT_FLOAT_EQ(position.x, 3.25f);
        EXPECT_FLOAT_EQ(position.y, 2.0f);
        EXPECT_NEAR(position.z, 44.516666f, 1e-4f);
    }

    std::filesystem::remove(scriptPath);
}

TEST(LuaScriptRuntime, EntityQueriesAndLog)
{
    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_queries", R"lua(
        local script = {}
        function script.on_create(entity)
            Ignite.Log("Logging from Lua")
            Ignite.LogWarn("Warning from Lua")
            Ignite.LogError("Error from Lua")

            local target = Ignite.FindEntityByName("TargetEntity")
            if target then
                local t = target:get_transform()
                t.position = { x = 99, y = 88, z = 77 }
            end
        end
        return script
    )lua");

    Scene scene;
    Entity runner = SceneManager::CreateEntity(&scene, "Runner", EntityType_Node);
    Entity target = SceneManager::CreateEntity(&scene, "TargetEntity", EntityType_Node);

    runner.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.CreateInstance(runner.GetUUID(), scriptPath);

        const auto &targetT = target.GetComponent<TransformComponent>().local;
        EXPECT_FLOAT_EQ(targetT.translation.x, 99.0f);
        EXPECT_FLOAT_EQ(targetT.translation.y, 88.0f);
        EXPECT_FLOAT_EQ(targetT.translation.z, 77.0f);
    }

    std::filesystem::remove(scriptPath);
}

TEST(LuaScriptRuntime, HotReloadAndErrorResilience)
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path scriptPath = std::filesystem::temp_directory_path() / std::format("ignite_hot_reload_{}.lua", unique);

    // Initial version v1: captures script-local state during initialization.
    {
        std::ofstream script(scriptPath);
        script << R"lua(
            local script = {}
            local offset = {}
            function script.on_create(entity)
                offset = entity:get_transform().position
            end
            function script.on_update(entity, dt)
                local t = entity:get_transform()
                t.position = { x = 10 + offset.x, y = 0, z = 0 }
            end
            return script
        )lua";
    }

    Scene scene;
    Entity entity = SceneManager::CreateEntity(&scene, "HotReloadEntity", EntityType_Node);
    entity.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.CreateInstance(entity.GetUUID(), scriptPath);

        runtime.Update(0.016f);
        EXPECT_FLOAT_EQ(entity.GetTransform().local.translation.x, 10.0f);

        // Update script on disk to v2: its local state must be initialized again.
        {
            std::ofstream script(scriptPath, std::ios::trunc);
            script << R"lua(
                local script = {}
                local offset = {}
                function script.on_create(entity)
                    offset = entity:get_transform().position
                end
                function script.on_update(entity, dt)
                    local t = entity:get_transform()
                    t.position = { x = 25 + offset.x, y = 0, z = 0 }
                end
                function script.on_reload(entity)
                    local t = entity:get_transform()
                    t.scale = { x = 7, y = 7, z = 7 }
                end
                return script
            )lua";
        }

        // Trigger reload
        EXPECT_TRUE(runtime.ReloadInstance(entity.GetUUID()));
        EXPECT_FLOAT_EQ(entity.GetTransform().local.scale.x, 7.0f);

        runtime.Update(0.016f);
        EXPECT_FLOAT_EQ(entity.GetTransform().local.translation.x, 35.0f);

        // Overwrite with syntax error
        {
            std::ofstream script(scriptPath, std::ios::trunc);
            script << "return { on_update = function( end";
        }

        // Reload should fail gracefully without crashing
        EXPECT_FALSE(runtime.ReloadInstance(entity.GetUUID()));

        // Subsequent update should continue with old valid callbacks without crashing
        runtime.Update(0.016f);
        EXPECT_FLOAT_EQ(entity.GetTransform().local.translation.x, 35.0f);
    }

    std::filesystem::remove(scriptPath);
}

TEST(LuaScriptAsset, AssetManagerAndAssetHandleIntegration)
{
    EXPECT_EQ(AssetTypeToString(AssetType::LuaScript), "LuaScript");
    EXPECT_EQ(AssetTypeFromString("LuaScript"), AssetType::LuaScript);
    EXPECT_EQ(GetAssetTypeFromExtension(".lua"), AssetType::LuaScript);
    EXPECT_EQ(GetAssetExtensionFromType(AssetType::LuaScript), ".lua");

    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_asset", "-- Hello Lua Asset\nreturn {}");

    Ref<lua::Script> scriptAsset = lua::Script::Create(scriptPath);
    ASSERT_NE(scriptAsset, nullptr);
    EXPECT_EQ(scriptAsset->GetAssetType(), AssetType::LuaScript);
    EXPECT_EQ(scriptAsset->GetFilepath(), scriptPath);
    EXPECT_FALSE(scriptAsset->GetSource().empty());
    EXPECT_TRUE(scriptAsset->IsReady());

    LuaScriptComponent component;
    EXPECT_EQ(component.scriptHandle, AssetHandle(0));
    EXPECT_TRUE(component.scriptPath.empty());

    component.scriptHandle = AssetHandle(123456789ULL);
    component.scriptPath = scriptPath;
    EXPECT_EQ(static_cast<uint64_t>(component.scriptHandle), 123456789ULL);
    EXPECT_EQ(component.scriptPath, scriptPath);

    std::filesystem::remove(scriptPath);
}

TEST(LuaScriptRuntime, TextAndDebugOverlayBindingsAndSerialization)
{
    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_text_debug", R"lua(
        local script = {}
        function script.on_create(entity)
            local tx = entity:get_component("Text")
            tx.text = "World Sign Label"
            tx.font_size = 32.0
            tx.billboard = true
            tx.center_align = true
            tx.screen_space = true
            tx.kerning = 1.5
            tx.line_spacing = 1.25
            tx.color = Ignite.Vec4(0.2, 0.8, 1.0, 0.9)

            Ignite.Debug.SetShowEntityNames(true)
            Ignite.Debug.DrawTextWorld(Ignite.Vec3(1.0, 2.0, 3.0), entity:get_name(), Ignite.Vec4(1, 1, 0, 1), 0.35, true, true)
            Ignite.Debug.DrawTextScreen(40.0, 80.0, "Screen Telemetry", Ignite.Vec4(0, 1, 0.5, 1), 20.0, true)
            Ignite.Debug.DrawLine(Ignite.Vec3(0, 0, 0), Ignite.Vec3(0, 5, 0), Ignite.Vec4(1, 0, 0, 1))
        end
        return script
    )lua");

    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteLuaTextSerializeTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    {
        ProjectInfo info;
        info.name = "TextSerializeTest";
        info.rootDirectory = tempDir;
        info.filepath = tempDir / "Test.ixproj";
        info.assetDirectory = "Assets";
        Ref<Project> proj = Project::Create(info);
        Ref<Scene> scene = CreateRef<Scene>(proj.get());

        Entity label = SceneManager::CreateEntity(scene.get(), "SignEntity", EntityType_Node);
        label.AddComponent<TextComponent>();
        label.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;

        {
            lua::ScriptRuntime runtime(scene.get());
            runtime.CreateInstance(label.GetUUID(), scriptPath);

            const auto &tx = label.GetComponent<TextComponent>();
            EXPECT_EQ(tx.text, "World Sign Label");
            EXPECT_FLOAT_EQ(tx.fontSize, 32.0f);
            EXPECT_TRUE(tx.billboard);
            EXPECT_TRUE(tx.centerAlign);
            EXPECT_TRUE(tx.screenSpace);
            EXPECT_FLOAT_EQ(tx.kerning, 1.5f);
            EXPECT_FLOAT_EQ(tx.lineSpacing, 1.25f);
            EXPECT_FLOAT_EQ(tx.color.r, 0.2f);
            EXPECT_FLOAT_EQ(tx.color.g, 0.8f);
            EXPECT_FLOAT_EQ(tx.color.b, 1.0f);
            EXPECT_FLOAT_EQ(tx.color.a, 0.9f);

            EXPECT_TRUE(scene->showDebugEntityNames);
            ASSERT_EQ(scene->GetDebugWorldTexts().size(), 1u);
            EXPECT_EQ(scene->GetDebugWorldTexts()[0].text, "SignEntity");
            EXPECT_TRUE(scene->GetDebugWorldTexts()[0].billboard);
            EXPECT_TRUE(scene->GetDebugWorldTexts()[0].center);
            ASSERT_EQ(scene->GetDebugScreenTexts().size(), 1u);
            EXPECT_EQ(scene->GetDebugScreenTexts()[0].text, "Screen Telemetry");
            EXPECT_FLOAT_EQ(scene->GetDebugScreenTexts()[0].fontSize, 20.0f);
            EXPECT_TRUE(scene->GetDebugScreenTexts()[0].center);
            ASSERT_EQ(scene->GetDebugLines().size(), 1u);

            scene->ClearDebugDraw();
            EXPECT_TRUE(scene->GetDebugWorldTexts().empty());
            EXPECT_TRUE(scene->GetDebugScreenTexts().empty());
            EXPECT_TRUE(scene->GetDebugLines().empty());
        }

        const std::filesystem::path scenePath = tempDir / "TextTest.ixscene";
        SceneSerializer serializer(scene, proj.get());
        ASSERT_TRUE(serializer.Serialize(scenePath));

        Ref<Scene> loaded = SceneSerializer::Deserialize(scenePath, proj.get());
        ASSERT_NE(loaded, nullptr);
        Entity found;
        auto view = loaded->registry->view<IDComponent>();
        for (entt::entity e : view)
        {
            if (view.get<IDComponent>(e).name == "SignEntity")
            {
                found = Entity{ e, loaded.get() };
                break;
            }
        }
        ASSERT_TRUE(found.IsValid());
        ASSERT_TRUE(found.HasComponent<TextComponent>());
        const auto &loadedTx = found.GetComponent<TextComponent>();
        EXPECT_EQ(loadedTx.text, "World Sign Label");
        EXPECT_FLOAT_EQ(loadedTx.fontSize, 32.0f);
        EXPECT_TRUE(loadedTx.billboard);
        EXPECT_TRUE(loadedTx.centerAlign);
        EXPECT_TRUE(loadedTx.screenSpace);
    }

    std::filesystem::remove(scriptPath, ec);
    std::filesystem::remove_all(tempDir, ec);
}

TEST(LuaScriptRuntime, SkeletalAnimatorAndCharacterRotationBindings)
{
    const std::filesystem::path scriptPath = CreateTempScript("ignite_lua_skel_cc", R"lua(
        local script = {}
        function script.on_create(entity)
            local cc = entity:get_component("CharacterController")
            cc:set_rotation(Ignite.Vec3(0.0, 90.0, 0.0))

            local skel = entity:get_component("SkeletalMesh")
            skel.unique_animator = true
            skel:set_float("Speed", 8.5)
            skel:set_bool("IsGrounded", false)
            skel:set_int("EmoteId", 2)
            skel:play_state("Jump")

            assert(math.abs(skel:get_float("Speed") - 8.5) < 0.001)
            assert(skel:get_bool("IsGrounded") == false)
            assert(skel:get_int("EmoteId") == 2)
            assert(skel.current_state == "Jump")
        end
        return script
    )lua");

    Scene scene;
    Entity bot = SceneManager::CreateEntity(&scene, "AnimBot", EntityType_Node);
    bot.AddComponent<CharacterControllerComponent>();
    auto &smc = bot.AddComponent<SkeletalMeshComponent>();
    smc.uniqueAnimator = true;
    smc.currentStateName = "Idle";

    bot.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.CreateInstance(bot.GetUUID(), scriptPath);

        // Verify CharacterController rotation update fell through to transform
        const glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(bot.GetComponent<TransformComponent>().local.rotation));
        EXPECT_NEAR(eulerDeg.y, 90.0f, 1.0f);

        // Verify SkeletalMeshComponent runtimeParams and current state triggered by Lua
        const auto *speedParam = Animator::FindAnimParam(smc.runtimeParams, "Speed");
        ASSERT_NE(speedParam, nullptr);
        EXPECT_FLOAT_EQ(speedParam->floatVal, 8.5f);
        const auto *groundedParam = Animator::FindAnimParam(smc.runtimeParams, "IsGrounded");
        ASSERT_NE(groundedParam, nullptr);
        EXPECT_FALSE(groundedParam->boolVal);
        const auto *emoteParam = Animator::FindAnimParam(smc.runtimeParams, "EmoteId");
        ASSERT_NE(emoteParam, nullptr);
        EXPECT_EQ(emoteParam->intVal, 2);

        EXPECT_EQ(smc.currentStateName, "Jump");
        EXPECT_FALSE(smc.isTransitioning);
    }

    std::error_code ec;
    std::filesystem::remove(scriptPath, ec);
}

