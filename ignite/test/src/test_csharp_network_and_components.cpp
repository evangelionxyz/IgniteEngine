// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/application.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/net/network_manager.hpp"
#include "ignite/net/net_replication.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace ignite;

TEST(NetReplication, LocalPlayerStaysVisibleOnConnectAndRestoresOnDisconnect)
{
    Ref<Scene> serverScene = Scene::Create(nullptr);
    Ref<Scene> clientScene = Scene::Create(nullptr);

    Entity serverTemplate = SceneManager::CreateEntity(serverScene.get(), "PlayerRobot", EntityType_Node);
    {
        auto &rc = serverTemplate.AddOrReplaceComponent<RenderingComponent>();
        rc.visible = true;
        auto &id = serverTemplate.AddComponent<NetworkIdentityComponent>();
        id.isLocalPlayer = true;
        id.replicate = true;
        id.prefab = "PlayerRobot";
    }

    Entity clientTemplate = SceneManager::CreateEntity(clientScene.get(), "PlayerRobot", EntityType_Node);
    {
        auto &rc = clientTemplate.AddOrReplaceComponent<RenderingComponent>();
        rc.visible = true;
        auto &id = clientTemplate.AddComponent<NetworkIdentityComponent>();
        id.isLocalPlayer = true;
        id.replicate = true;
        id.prefab = "PlayerRobot";
    }

    NetServerSystem::Config cfg;
    cfg.port = 18944;
    NetServerSystem serverSys(cfg);
    ASSERT_TRUE(serverSys.Start());
    NetServerReplication serverRep(serverScene.get(), &serverSys);

    NetClientSystem clientSys;
    auto clientRep = std::make_unique<NetClientReplication>(clientScene.get(), &clientSys);

    // Connecting must NOT hide the local player character
    ASSERT_TRUE(clientTemplate.HasComponent<RenderingComponent>());
    EXPECT_TRUE(clientTemplate.GetComponent<RenderingComponent>().visible);

    // Connect and verify spawned avatar stays visible
    serverSys.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &c) {
        serverRep.SyncWorldToClient(c.clientId);
        serverRep.SpawnEntity("PlayerRobot", glm::vec3(2.0f, 1.0f, -3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), c.clientId);
    });

    clientSys.Connect("127.0.0.1:" + std::to_string(cfg.port), "HeroClient");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline &&
           (!clientSys.IsConnected() || !clientRep->GetLocalPlayerEntity().IsValid() || clientRep->GetLocalPlayerEntity().GetComponent<NetworkIdentityComponent>().netId == 0))
    {
        serverSys.Tick();
        clientSys.Tick();
        clientRep->Update(1.0f / 60.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(clientSys.IsConnected());
    Entity claimed = clientRep->GetLocalPlayerEntity();
    ASSERT_TRUE(claimed.IsValid());
    EXPECT_TRUE(claimed.GetComponent<RenderingComponent>().visible);

    // Disconnect cleanup preserves visible state and clears runtime netId
    clientSys.Disconnect();
    clientRep.reset();
    serverSys.Stop();

    EXPECT_TRUE(clientTemplate.GetComponent<RenderingComponent>().visible);
    EXPECT_EQ(clientTemplate.GetComponent<NetworkIdentityComponent>().netId, 0u);
    EXPECT_TRUE(clientTemplate.GetComponent<NetworkIdentityComponent>().isLocalPlayer);
}

TEST(CSharpScripting, ExposesServerNetworkAndNewComponentsEndToEnd)
{
    const std::filesystem::path projectDir = vfs::GetExecutableDirectory() / "test-resources" / "temp" / "CSharpNetAndCompTest";
    std::error_code ec;
    std::filesystem::remove_all(projectDir, ec);
    std::filesystem::create_directories(projectDir, ec);

    ProjectInfo info;
    info.name = "CSharpNetAndCompTest";
    info.rootDirectory = projectDir;
    info.filepath = projectDir / "CSharpNetAndCompTest.ixproj";
    info.configuration = ProjectConfiguration::Debug;

    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    const std::filesystem::path scriptPath = project->GetScriptsDirectory() / "NetComponentHostScript.cs";
    {
        std::ofstream out(scriptPath);
        out << R"(using Ignite;
using static Ignite.Mathf;

namespace CSharpNetAndCompTest;

public class NetComponentHostScript : Entity
{
    public bool ComponentsVerified = false;
    public bool ServerStartedFromCSharp = false;
    public bool JoinedCalled = false;
    public uint JoinedClientId = 0;
    public bool JoinedNameMatched = false;
    public uint ClientCountObserved = 0;
    public bool ConnectedCalled = false;
    public bool InputCalled = false;
    public ushort LastButtons = 0;
    public bool LeftCalled = false;
    public bool ServerStoppedFromCSharp = false;

    public override void OnCreate()
    {
        var rend = AddComponent<RenderingComponent>();
        rend.Visible = false;
        rend.Visible = true;

        var cam = AddComponent<CameraComponent>();
        cam.Primary = true;
        cam.ProjectionType = CameraProjectionType.Perspective;
        cam.Fov = 72.0f;
        cam.NearClip = 0.25f;
        cam.FarClip = 750.0f;

        var dir = AddComponent<DirectionalLightComponent>();
        dir.Intensity = 4.5f;
        dir.ShadowDistance = 120.0f;

        var pt = AddComponent<PointLightComponent>();
        pt.Range = 22.0f;
        pt.Intensity = 3.2f;

        var spot = AddComponent<SpotLightComponent>();
        spot.InnerConeAngle = 15.0f;
        spot.OuterConeAngle = 40.0f;

        var pt2d = AddComponent<PointLight2DComponent>();
        pt2d.Radius = 9.0f;

        var mesh = AddComponent<StaticMeshComponent>();
        mesh.SetOverrideMaterial(0, 12345ul);

        var env = AddComponent<WorldEnvironmentComponent>();
        env.Exposure = 1.4f;
        env.FogDensity = 0.05f;
        env.MarkDirty();

        var ps = AddComponent<ParticleSystemComponent>();
        ps.ApplyPreset(ParticleEmitterPreset.Fire);
        ps.EmissionRate = 95.0f;
        ps.Play();

        var netId = AddComponent<NetworkIdentityComponent>();
        netId.Replicate = true;
        netId.Prefab = "CSharpHero";
        netId.MarkDirty(NetCompFlag.Transform);

        var planeCol = AddComponent<PlaneColliderComponent>();
        planeCol.Scale = new Vector3(4.0f, 1.0f, 4.0f);

        var txt = AddComponent<TextComponent>();
        txt.TextString = "NetUI";
        txt.FontSize = 32.0f;
        txt.ScreenSpace = true;

        RemoveComponent<PointLight2DComponent>();

        ComponentsVerified =
            rend.Visible == true &&
            cam.Primary && cam.Fov > 71.0f &&
            dir.Intensity > 4.4f &&
            pt.Range > 21.0f &&
            spot.OuterConeAngle > 39.0f &&
            mesh.GetOverrideMaterial(0) == 12345ul &&
            env.FogDensity > 0.04f &&
            ps.IsPlaying && ps.EmissionRate > 94.0f &&
            netId.Prefab == "CSharpHero" &&
            planeCol.Scale.X > 3.9f &&
            txt.ScreenSpace && txt.FontSize > 31.0f &&
            !HasComponent<PointLight2DComponent>();

        ServerStartedFromCSharp = Net.StartServer(18945, 60, 20) && Net.IsServerRunning;
    }

    public override void OnNetworkClientJoined(uint clientId, string clientName)
    {
        JoinedCalled = true;
        JoinedClientId = clientId;
        JoinedNameMatched = (clientName == "CSharpPlayer");
        Net.SpawnEntity("CSharpHero", new Vector3(1.0f, 2.0f, 3.0f), 90.0f, 0.0f, clientId);
        ClientCountObserved = Net.GetClientCount();
        var connected = Net.GetConnectedClients();
        if (connected.Length > 0 && connected[0].Name == "CSharpPlayer")
        {
            Net.SendChat("Welcome from C# Server");
        }
    }

    public override void OnNetworkClientLeft(uint clientId, string clientName)
    {
        LeftCalled = true;
    }

    public override void OnNetworkConnected(uint clientId)
    {
        ConnectedCalled = true;
    }

    public override void OnNetworkInput(uint clientId, float moveX, float moveY, ushort buttons, float yaw, float deltaTime)
    {
        InputCalled = true;
        LastButtons = buttons;
    }

    public override void OnDestroy()
    {
        if (Net.IsServerRunning)
        {
            Net.StopServer();
            ServerStoppedFromCSharp = !Net.IsServerRunning;
        }
    }
}
)";
    }

    project->RegenerateCSharpProject();
    ASSERT_TRUE(project->BuildSolution(false));
    project->InitScriptEngine();

    ScriptEngine *se = project->GetScriptEngine();
    ASSERT_NE(se, nullptr);

    for (int i = 0; i < 50 && !se->IsReady(); ++i)
    {
        if (auto *app = Application::GetInstance())
            app->ProcessMainThreadSubmissions();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(se->IsReady());
    ASSERT_TRUE(se->IsEntityClassExists("CSharpNetAndCompTest.NetComponentHostScript"));

    Ref<Scene> scene = Scene::Create(project.get());
    se->SetSceneContext(scene.get());

    NetworkManager *nm = NetworkManager::GetInstance();
    ASSERT_NE(nm, nullptr);
    nm->SetActiveScene(scene.get());

    Entity hostEntity = SceneManager::CreateEntity(scene.get(), "HostController", EntityType_Node);
    auto &sc = hostEntity.AddComponent<ScriptComponent>();
    sc.className = "CSharpNetAndCompTest.NetComponentHostScript";

    scene->OnStart(ESceneState::Play);
    ASSERT_NE(sc.runtimeScriptInstance, nullptr);
    EXPECT_TRUE(sc.runtimeScriptInstance->GetFieldValue<bool>("ComponentsVerified"));
    EXPECT_TRUE(sc.runtimeScriptInstance->GetFieldValue<bool>("ServerStartedFromCSharp"));
    EXPECT_TRUE(nm->IsServerRunning());

    // Simulate client connection on loopback via separate NetClientSystem so host OnNetworkClientJoined fires
    NetClientSystem client;
    client.Connect("127.0.0.1:18945", "CSharpPlayer");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !sc.runtimeScriptInstance->GetFieldValue<bool>("JoinedCalled"))
    {
        nm->Update(1.0f / 60.0f);
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(sc.runtimeScriptInstance->GetFieldValue<bool>("JoinedCalled"));
    EXPECT_TRUE(sc.runtimeScriptInstance->GetFieldValue<bool>("JoinedNameMatched"));
    EXPECT_GT(sc.runtimeScriptInstance->GetFieldValue<uint32_t>("JoinedClientId"), 0u);
    EXPECT_EQ(sc.runtimeScriptInstance->GetFieldValue<uint32_t>("ClientCountObserved"), 1u);

    const uint32_t joinedId = sc.runtimeScriptInstance->GetFieldValue<uint32_t>("JoinedClientId");
    Entity spawnedPlayer;
    auto view = scene->registry->view<NetworkIdentityComponent>();
    for (entt::entity e : view)
    {
        if (view.get<NetworkIdentityComponent>(e).ownerClient == joinedId)
        {
            spawnedPlayer = Entity{ e, scene.get() };
            break;
        }
    }
    ASSERT_TRUE(spawnedPlayer.IsValid());

    ScriptEngine::InvokeNetworkInput(hostEntity.GetUUID(), joinedId, 0.5f, -0.5f, netproto::InputButton_Jump, 45.0f, 1.0f / 60.0f);
    EXPECT_TRUE(sc.runtimeScriptInstance->GetFieldValue<bool>("InputCalled"));
    EXPECT_EQ(sc.runtimeScriptInstance->GetFieldValue<uint16_t>("LastButtons"), static_cast<uint16_t>(netproto::InputButton_Jump));

    client.Disconnect();
    const auto leftDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < leftDeadline && !sc.runtimeScriptInstance->GetFieldValue<bool>("LeftCalled"))
    {
        nm->Update(1.0f / 60.0f);
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(sc.runtimeScriptInstance->GetFieldValue<bool>("LeftCalled"));

    nm->SetActiveScene(nullptr);
    scene->OnStop();
    EXPECT_FALSE(nm->IsServerRunning());
    se->ClearSceneContext();
    Project::CloseActive();
    std::filesystem::remove_all(projectDir, ec);
}
