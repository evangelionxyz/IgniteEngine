// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/scripting/lua/lua_script_runtime.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <format>

using namespace ignite;

namespace
{
    std::filesystem::path CreateTempLuaFile(std::string_view prefix, std::string_view content)
    {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path path = std::filesystem::temp_directory_path() / std::format("{}_{}.lua", prefix, unique);
        std::ofstream ofs(path);
        ofs << content;
        return path;
    }

    Entity FindEntityByName(Scene *scene, std::string_view name)
    {
        auto view = scene->registry->view<IDComponent>();
        for (entt::entity e : view)
        {
            if (view.get<IDComponent>(e).name == name)
                return Entity{ e, scene };
        }
        return {};
    }
}

TEST(LuaEnhancements, VecMathHierarchyAndPrimitivesSnippet)
{
    Scene scene;
    lua::ScriptRuntime runtime(&scene);

    std::string result;
    std::string err;
    const bool ok = runtime.ExecuteSnippet(R"lua(
        local a = Ignite.Vec3(1, 2, 3)
        local b = Ignite.Vec3(4, -2, 1)
        local c = (a + b) * 2
        assert(math.abs(c.x - 10) < 0.001)
        assert(math.abs(a:dot(b) - 3) < 0.001)
        local cr = Ignite.Vec3(1, 0, 0):cross(Ignite.Vec3(0, 1, 0))
        assert(math.abs(cr.z - 1) < 0.001)
        assert(math.abs(Ignite.Math.Clamp(12, 0, 10) - 10) < 0.001)

        local parent = Ignite.CreateEntity("ParentNode")
        local child = Ignite.CreatePrimitive("Sphere", "ChildSphere")
        child:set_parent(parent)
        local tr = child:get_component("Transform")
        tr:translate(Ignite.Vec3(3, 4, 5))

        local found = Ignite.FindEntityByName("ChildSphere")
        assert(found ~= nil and found:is_valid())
        assert(found:get_parent().name == "ParentNode")
        assert(#parent:get_children() == 1)

        local meshes = Ignite.GetEntitiesWith("StaticMesh")
        assert(#meshes >= 1)

        return found:get_component("Transform").position.x
    )lua", result, err);

    EXPECT_TRUE(ok) << err;
    EXPECT_TRUE(err.empty());
    EXPECT_NEAR(std::stof(result), 3.0f, 0.01f);

    Entity child = FindEntityByName(&scene, "ChildSphere");
    ASSERT_TRUE(static_cast<bool>(child));
    EXPECT_TRUE(child.HasComponent<StaticMeshComponent>());
    EXPECT_TRUE(child.HasComponent<TransformComponent>());
    EXPECT_TRUE(child.GetComponent<TransformComponent>().dirty);
}

TEST(LuaEnhancements, FixedDeltaTimeAndCollisionCallbacks)
{
    const std::filesystem::path scriptPath = CreateTempLuaFile("ignite_lua_collision", R"lua(
        local script = {}
        function script.on_create(entity)
            fixed_dt_acc = 0.0
            collided_other = ""
            collided_normal_y = 0.0
        end
        function script.on_fixed_update(entity, dt)
            fixed_dt_acc = fixed_dt_acc + dt
            local tr = entity:get_component("Transform")
            tr.position = Ignite.Vec3(fixed_dt_acc, tr.position.y, tr.position.z)
        end
        function script.on_collision_enter(entity, other, point, normal)
            collided_other = other and other.name or ""
            collided_normal_y = normal.y
            local tr = entity:get_component("Transform")
            tr.position = Ignite.Vec3(tr.position.x, 9.5, tr.position.z)
        end
        return script
    )lua");

    Scene scene;
    Entity mover = SceneManager::CreateEntity(&scene, "Mover", EntityType_Node);
    Entity floor = SceneManager::CreateEntity(&scene, "Floor", EntityType_Node);
    mover.AddComponent<LuaScriptComponent>().scriptPath = scriptPath;

    {
        lua::ScriptRuntime runtime(&scene);
        runtime.CreateInstance(mover.GetUUID(), scriptPath);

        runtime.FixedUpdate(0.025f);
        runtime.FixedUpdate(0.025f);
        EXPECT_NEAR(mover.GetTransform().local.translation.x, 0.05f, 0.001f);

        runtime.InvokeCollision(mover.GetUUID(), floor.GetUUID(), "on_collision_enter",
            glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        EXPECT_NEAR(mover.GetTransform().local.translation.y, 9.5f, 0.001f);
    }

    std::error_code ec;
    std::filesystem::remove(scriptPath, ec);
}

TEST(LuaEnhancements, SnippetCapturesErrorsSafely)
{
    Scene scene;
    lua::ScriptRuntime runtime(&scene);

    std::string outResult;
    std::string outError;
    EXPECT_FALSE(runtime.ExecuteSnippet("this is invalid lua syntax (", outResult, outError));
    EXPECT_FALSE(outError.empty());

    outError.clear();
    EXPECT_FALSE(runtime.ExecuteSnippet("error('intentional runtime error')", outResult, outError));
    EXPECT_NE(outError.find("intentional runtime error"), std::string::npos);
}
