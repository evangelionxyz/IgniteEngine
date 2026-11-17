// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/scripting/lua/lua_script_runtime.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/project/project.hpp"

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

TEST(LuaEnhancements, PhysicsJointAndParticleSystemBindingsAndNetStats)
{
    ProjectInfo pInfo;
    pInfo.name = "LuaJointTestProj";
    pInfo.filepath = std::filesystem::temp_directory_path() / "LuaJointTestProj.ixproj";
    pInfo.rootDirectory = std::filesystem::temp_directory_path();
    Ref<Project> project = Project::Create(pInfo);
    Scene scene(project.get());
    lua::ScriptRuntime runtime(&scene);

    std::string outResult;
    std::string outError;
    const bool ok = runtime.ExecuteSnippet(R"lua(
        -- 1. Test Physics Joint bindings
        local anchor = Ignite.CreateEntity("AnchorBeam")
        local bob = Ignite.CreateEntity("BobSphere")
        local joint = bob:add_component("PhysicsJoint")
        assert(joint ~= nil, "joint component was nil")
        assert(bob:has_component("PhysicsJoint"), "has_component PhysicsJoint failed")

        joint.connected_entity = anchor.id
        joint.joint_type = "Hinge"
        joint.enable_limits = true
        joint.limits_min = -60.0
        joint.limits_max = 60.0
        joint.motor_state = "Velocity"
        joint.motor_velocity = 35.0
        joint.max_motor_force = 2500.0
        assert(joint.joint_type == "Hinge", "joint_type did not match")
        assert(joint.motor_state == "Velocity", "motor_state did not match")

        -- 2. Test Particle System bindings
        local emitter = Ignite.CreateEntity("TorchParticle")
        local ps = emitter:add_component("ParticleSystem")
        assert(ps ~= nil, "particle component was nil")
        assert(emitter:has_component("ParticleSystem"), "has_component ParticleSystem failed")
        assert(emitter:has_component("Rendering"), "particle system should auto-ensure RenderingComponent")

        ps:set_preset("Fire")
        assert(ps.type == "Fire", "particle preset Fire failed")
        ps.max_particles = 3200
        ps.emission_rate = 95.0
        ps.is_playing = true
        assert(ps.max_particles == 3200, "max_particles mismatch")
        assert(math.abs(ps.emission_rate - 95.0) < 0.01, "emission_rate mismatch")
        ps:stop()
        assert(ps.is_playing == false, "particle stop failed")
        ps:play()
        assert(ps.is_playing == true, "particle play failed")

        -- 3. Test Net stats bindings
        local stats = Ignite.Net.GetStats()
        assert(type(stats) == "table", "GetStats did not return table")
        assert(stats.rtt ~= nil, "GetStats missing rtt")
        assert(stats.packet_loss ~= nil, "GetStats missing packet_loss")
        assert(Ignite.Net.GetClientCount() ~= nil, "GetClientCount failed")

        return "OK"
    )lua", outResult, outError);

    EXPECT_TRUE(ok) << outError;
    EXPECT_EQ(outResult, "OK");

    Entity bob = FindEntityByName(&scene, "BobSphere");
    ASSERT_TRUE(static_cast<bool>(bob));
    ASSERT_TRUE(bob.HasComponent<PhysicsJointComponent>());
    const auto &jc = bob.GetComponent<PhysicsJointComponent>();
    EXPECT_EQ(jc.desc.type, physics::JointType::Hinge);
    EXPECT_TRUE(jc.desc.enableLimits);
    EXPECT_NEAR(jc.desc.limitsMin, -60.0f, 0.01f);
    EXPECT_NEAR(jc.desc.limitsMax, 60.0f, 0.01f);
    EXPECT_EQ(jc.desc.motorState, physics::JointMotorState::Velocity);
    EXPECT_NEAR(jc.desc.motorTargetVelocity, 35.0f, 0.01f);
    EXPECT_TRUE(jc.dirty);

    Entity emitter = FindEntityByName(&scene, "TorchParticle");
    ASSERT_TRUE(static_cast<bool>(emitter));
    ASSERT_TRUE(emitter.HasComponent<ParticleSystemComponent>());
    ASSERT_TRUE(emitter.HasComponent<RenderingComponent>());
    const auto &pc = emitter.GetComponent<ParticleSystemComponent>();
    EXPECT_EQ(pc.settings.type, ParticleEmitterType::Fire);
    EXPECT_EQ(pc.settings.maxParticles, 3200u);
    EXPECT_NEAR(pc.settings.emissionRate, 95.0f, 0.01f);
    EXPECT_TRUE(pc.isPlaying);

    // Setup rigidbodies and colliders for anchor and bob, then verify Scene::Start() runs simulation
    Entity anchor = FindEntityByName(&scene, "AnchorBeam");
    ASSERT_TRUE(static_cast<bool>(anchor));
    auto &rbAnchor = anchor.AddComponent<RigidbodyComponent>();
    rbAnchor.bodyType = physics::BodyType::Static;
    auto &colAnchor = anchor.AddComponent<BoxColliderComponent>();
    colAnchor.scale = glm::vec3(0.5f);

    auto &rbBob = bob.AddComponent<RigidbodyComponent>();
    rbBob.bodyType = physics::BodyType::Dynamic;
    rbBob.mass = 10.0f;
    auto &colBob = bob.AddComponent<SphereColliderComponent>();
    colBob.radius = 0.5f;

    scene.OnStart(ESceneState::Play);
    scene.OnUpdateRuntimeSimulate(0.016f);
    EXPECT_FALSE(jc.runtimeJoint.expired());
    scene.OnStop();
}
