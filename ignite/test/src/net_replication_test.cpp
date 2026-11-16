// Copyright (c) 2026 Evangelion Manuhutu
// Google Test suite for Entity Replication & Transform Interpolation:
// 1. Authoritative entity spawning & NetworkIdentityComponent binding
// 2. Late joiner world state synchronization (SyncWorldToClient)
// 3. Transform snapshot broadcasting
// 4. Remote transform interpolation & antipodal quaternion shortest path
// 5. Linear extrapolation within safety bounds
// 6. Entity despawning & client scene cleanup

#include <gtest/gtest.h>

#include <ignite/net/net_server_system.hpp>
#include <ignite/net/net_client_system.hpp>
#include <ignite/net/net_replication.hpp>
#include <ignite/net/net_interpolator.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/scene/entity.hpp>
#include <ignite/scene/component.hpp>
#include <ignite/scene/scene_manager.hpp>

#include <chrono>
#include <thread>
#include <cmath>

using namespace ignite;

TEST(NetReplication, ServerAssignsFreshIdsAndKeepsHostPlayerReplicated)
{
    Scene scene;
    Entity hostPlayer = SceneManager::CreateEntity(&scene, "HostPlayer", EntityType_Node);
    auto &hostIdentity = hostPlayer.AddComponent<NetworkIdentityComponent>();
    hostIdentity.netId = 888;
    scene.RegisterNetEntity(hostIdentity.netId, hostPlayer);
    hostIdentity.isLocalPlayer = true;

    Entity localOnly = SceneManager::CreateEntity(&scene, "LocalOnly", EntityType_Node);
    auto &localOnlyIdentity = localOnly.AddComponent<NetworkIdentityComponent>();
    localOnlyIdentity.replicate = false;

    NetServerSystem server(NetServerSystem::Config{});
    NetServerReplication replication(&scene, &server);

    EXPECT_NE(hostIdentity.netId, 0u);
    EXPECT_EQ(hostIdentity.ownerClient, 0u);
    EXPECT_TRUE(hostIdentity.isLocalPlayer);
    EXPECT_TRUE(hostIdentity.replicate);
    EXPECT_NE(hostIdentity.netId, 888u);
    EXPECT_EQ(scene.FindEntityByNetId(hostIdentity.netId), hostPlayer);
    EXPECT_FALSE(scene.FindEntityByNetId(888).IsValid());
    EXPECT_EQ(localOnlyIdentity.netId, 0u);
}

TEST(NetReplication, SnapshotUpdatesRemoteCharacterMovementState)
{
    Scene scene;
    NetClientSystem client;
    NetClientReplication replication(&scene, &client);

    Entity remotePlayer = SceneManager::CreateEntity(&scene, "RemotePlayer", EntityType_Node);
    auto &identity = remotePlayer.AddComponent<NetworkIdentityComponent>();
    identity.netId = 42;
    scene.RegisterNetEntity(identity.netId, remotePlayer);

    netproto::SnapshotMsg snapshot;
    netproto::EntityState state;
    state.netId = identity.netId;
    state.movementSpeed = 0.0f;
    state.isGrounded = true;
    snapshot.entities.push_back(state);

    ASSERT_TRUE(client.onSnapshot);
    client.onSnapshot(snapshot);

    EXPECT_FLOAT_EQ(identity.replicatedMovementSpeed, 0.0f);
    EXPECT_TRUE(identity.replicatedIsGrounded);
}

TEST(NetReplication, BindsReplicatedPlatformToAuthoredClientEntity)
{
    NetServerSystem::Config config;
    config.port = 8215;
    NetServerSystem server(config);
    ASSERT_TRUE(server.Start());

    Scene serverScene;
    Entity serverPlatform = SceneManager::CreateEntity(&serverScene, "Platform_Elevator", EntityType_Node);
    serverPlatform.AddComponent<TransformComponent>();
    auto &serverIdentity = serverPlatform.AddComponent<NetworkIdentityComponent>();
    serverIdentity.prefab = "Platform_Elevator";
    serverIdentity.replicate = true;
    NetServerReplication serverReplication(&serverScene, &server);
    const uint64_t platformNetId = serverIdentity.netId;

    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &client)
    {
        serverReplication.SyncWorldToClient(client.clientId);
    });

    Scene clientScene;
    Entity authoredPlatform = SceneManager::CreateEntity(&clientScene, "Platform_Elevator", EntityType_Node);
    authoredPlatform.AddComponent<TransformComponent>();
    auto &clientIdentity = authoredPlatform.AddComponent<NetworkIdentityComponent>();
    clientIdentity.prefab = "Platform_Elevator";

    NetClientSystem client;
    NetClientReplication clientReplication(&clientScene, &client);
    size_t spawnCount = 0;
    const auto onSpawn = client.onSpawn;
    client.onSpawn = [&](const netproto::SpawnMsg &msg)
    {
        ++spawnCount;
        onSpawn(msg);
    };
    bool connected = false;
    client.onConnected = [&]() { connected = true; };
    client.Connect("127.0.0.1:8215", "PlatformTest");

    for (int i = 0; i < 100 && !connected; ++i)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(connected);
    ASSERT_EQ(server.GetClientCount(), 1u);

    for (int i = 0; i < 10; ++i)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(spawnCount, 1u);
    EXPECT_EQ(clientIdentity.netId, platformNetId);
    Entity replicatedPlatform = clientScene.FindEntityByNetId(platformNetId);
    EXPECT_EQ(replicatedPlatform, authoredPlatform);
    EXPECT_EQ(clientScene.registry->view<NetworkIdentityComponent>().size(), 1u);

    client.Disconnect();
    server.Stop();
}

TEST(NetReplication, HostAndTwoClientsReceiveEveryPlayerAvatar)
{
    NetServerSystem::Config config;
    config.port = 8214;
    NetServerSystem server(config);
    ASSERT_TRUE(server.Start());

    Scene serverScene;
    Entity hostPlayer = SceneManager::CreateEntity(&serverScene, "HostPlayer", EntityType_Node);
    hostPlayer.AddComponent<TransformComponent>();
    hostPlayer.GetTransform().local.scale = glm::vec3(0.48f);
    hostPlayer.GetTransform().world.scale = glm::vec3(0.48f);
    hostPlayer.AddComponent<RenderingComponent>();
    auto &hostIdentity = hostPlayer.AddComponent<NetworkIdentityComponent>();
    hostIdentity.isLocalPlayer = true;
    NetServerReplication serverReplication(&serverScene, &server);
    const uint64_t hostNetId = hostIdentity.netId;
    std::vector<uint64_t> clientNetIds;
    std::vector<glm::vec3> clientSpawnPositions;
    std::vector<glm::quat> clientSpawnRotations;

    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &clientInfo)
    {
        serverReplication.SyncWorldToClient(clientInfo.clientId);
        const glm::vec3 spawnPosition(2.0f * clientInfo.clientId, 1.0f, -3.0f);
        const glm::quat spawnRotation = glm::angleAxis(glm::radians(10.0f * clientInfo.clientId), glm::vec3(0.0f, 1.0f, 0.0f));
        Entity player = serverReplication.SpawnEntity("HostPlayer", spawnPosition, spawnRotation, clientInfo.clientId);
        if (player.IsValid())
        {
            clientNetIds.push_back(player.GetComponent<NetworkIdentityComponent>().netId);
            clientSpawnPositions.push_back(spawnPosition);
            clientSpawnRotations.push_back(spawnRotation);
        }
    });

    auto createClientTemplate = [](Scene &scene)
    {
        Entity entity = SceneManager::CreateEntity(&scene, "HostPlayer", EntityType_Node);
        entity.AddComponent<TransformComponent>();
        entity.GetTransform().local.scale = glm::vec3(0.48f);
        entity.GetTransform().world.scale = glm::vec3(0.48f);
        entity.AddComponent<RenderingComponent>();
        entity.AddComponent<NetworkIdentityComponent>().isLocalPlayer = true;
    };

    Scene clientSceneB;
    createClientTemplate(clientSceneB);
    Scene clientSceneC;
    createClientTemplate(clientSceneC);

    NetClientSystem clientB;
    NetClientReplication clientReplicationB(&clientSceneB, &clientB);
    bool connectedB = false;
    clientB.onConnected = [&]() { connectedB = true; };
    clientB.Connect("127.0.0.1:8214", "ClientB");

    for (int i = 0; i < 100 && !connectedB; ++i)
    {
        server.Tick();
        clientB.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(connectedB);

    NetClientSystem clientC;
    NetClientReplication clientReplicationC(&clientSceneC, &clientC);
    bool connectedC = false;
    clientC.onConnected = [&]() { connectedC = true; };
    clientC.Connect("127.0.0.1:8214", "ClientC");

    for (int i = 0; i < 100 && !connectedC; ++i)
    {
        server.Tick();
        clientB.Tick();
        clientC.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(connectedC);

    for (int i = 0; i < 20; ++i)
    {
        server.Tick();
        clientB.Tick();
        clientC.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_EQ(clientNetIds.size(), 2u);
    const uint64_t clientBNetId = clientNetIds[0];
    const uint64_t clientCNetId = clientNetIds[1];
    auto clientBPlayerOnServer = serverScene.FindEntityByNetId(clientBNetId);
    auto clientCPlayerOnServer = serverScene.FindEntityByNetId(clientCNetId);
    ASSERT_TRUE(clientBPlayerOnServer.IsValid());
    ASSERT_TRUE(clientCPlayerOnServer.IsValid());
    EXPECT_TRUE(clientBPlayerOnServer.GetComponent<RenderingComponent>().visible);
    EXPECT_TRUE(clientCPlayerOnServer.GetComponent<RenderingComponent>().visible);
    EXPECT_FLOAT_EQ(clientBPlayerOnServer.GetTransform().world.scale.x, 0.48f);
    EXPECT_FLOAT_EQ(clientCPlayerOnServer.GetTransform().world.scale.x, 0.48f);
    EXPECT_NEAR(glm::distance(clientBPlayerOnServer.GetTransform().world.translation, clientSpawnPositions[0]), 0.0f, 1e-5f);
    EXPECT_NEAR(glm::distance(clientCPlayerOnServer.GetTransform().world.translation, clientSpawnPositions[1]), 0.0f, 1e-5f);
    EXPECT_NEAR(glm::abs(glm::dot(clientBPlayerOnServer.GetTransform().world.rotation, clientSpawnRotations[0])), 1.0f, 1e-5f);
    EXPECT_NEAR(glm::abs(glm::dot(clientCPlayerOnServer.GetTransform().world.rotation, clientSpawnRotations[1])), 1.0f, 1e-5f);

    Entity playerB = clientReplicationB.GetLocalPlayerEntity();
    Entity playerC = clientReplicationC.GetLocalPlayerEntity();
    Entity hostOnB = clientSceneB.FindEntityByNetId(hostNetId);
    Entity clientCOnB = clientSceneB.FindEntityByNetId(clientCNetId);
    Entity hostOnC = clientSceneC.FindEntityByNetId(hostNetId);
    Entity clientBOnC = clientSceneC.FindEntityByNetId(clientBNetId);
    ASSERT_TRUE(playerB.IsValid());
    ASSERT_TRUE(playerC.IsValid());
    ASSERT_TRUE(hostOnB.IsValid());
    ASSERT_TRUE(clientCOnB.IsValid());
    ASSERT_TRUE(hostOnC.IsValid());
    ASSERT_TRUE(clientBOnC.IsValid());
    EXPECT_NE(playerB, clientCOnB);
    EXPECT_NE(playerC, clientBOnC);
    EXPECT_TRUE(playerB.GetComponent<NetworkIdentityComponent>().isLocalPlayer);
    EXPECT_TRUE(playerC.GetComponent<NetworkIdentityComponent>().isLocalPlayer);
    EXPECT_FALSE(clientCOnB.GetComponent<NetworkIdentityComponent>().isLocalPlayer);
    EXPECT_FALSE(clientBOnC.GetComponent<NetworkIdentityComponent>().isLocalPlayer);
    EXPECT_TRUE(hostOnB.GetComponent<RenderingComponent>().visible);
    EXPECT_TRUE(clientCOnB.GetComponent<RenderingComponent>().visible);
    EXPECT_TRUE(hostOnC.GetComponent<RenderingComponent>().visible);
    EXPECT_TRUE(clientBOnC.GetComponent<RenderingComponent>().visible);
    EXPECT_FLOAT_EQ(clientCOnB.GetTransform().world.scale.x, 0.48f);
    EXPECT_FLOAT_EQ(clientBOnC.GetTransform().world.scale.x, 0.48f);

    clientB.Disconnect();
    clientC.Disconnect();
    server.Stop();
}

TEST(NetReplication, EntitySpawnAndNetworkIdentity)
{
    NetServerSystem::Config cfg;
    cfg.port = 8211;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    Scene serverScene;
    NetServerReplication serverRep(&serverScene, &server);

    Entity e1 = serverRep.SpawnEntity("MonsterPrefab", glm::vec3(10.0f, 2.0f, 5.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 0);
    ASSERT_TRUE(e1.IsValid());
    ASSERT_TRUE(e1.HasComponent<NetworkIdentityComponent>());

    const auto &netComp = e1.GetComponent<NetworkIdentityComponent>();
    EXPECT_GE(netComp.netId, 1000u);
    EXPECT_EQ(netComp.ownerClient, 0u);
    EXPECT_EQ(netComp.prefab, "MonsterPrefab");
    EXPECT_TRUE(netComp.replicate);

    // Verify entity can be queried by NetID
    Entity found = serverScene.FindEntityByNetId(netComp.netId);
    EXPECT_EQ(found, e1);

    server.Stop();
}

TEST(NetReplication, RemoteTransformInterpolation)
{
    NetworkTransformInterpolator interp(0.1, 0.15); // 100ms delay, 150ms max extrap

    // Add 2 snapshots: t=0.0 at (0, 0, 0) and t=0.2 at (10, 0, 0)
    interp.AddSnapshot(1, 0.0, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    interp.AddSnapshot(2, 0.2, glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

    EXPECT_EQ(interp.GetSnapshotCount(), 2u);

    // Current time = 0.2, renderTime = 0.2 - 0.1 = 0.1 (exact midpoint)
    glm::vec3 pos, scale;
    glm::quat rot;
    ASSERT_TRUE(interp.Sample(0.2, pos, rot, scale));

    EXPECT_NEAR(pos.x, 5.0f, 0.01f);
    EXPECT_NEAR(pos.y, 0.0f, 0.01f);
    EXPECT_NEAR(pos.z, 0.0f, 0.01f);
    EXPECT_NEAR(scale.x, 1.0f, 0.01f);
}

TEST(NetReplication, InterpolatorAntipodalQuaternionShortestPath)
{
    NetworkTransformInterpolator interp(0.05, 0.1);

    glm::quat q0 = glm::angleAxis(glm::radians(10.0f), glm::vec3(0, 1, 0));
    // q1 is a 20-degree rotation, but represented with opposite sign: -q1 represents the EXACT SAME orientation
    glm::quat q1 = -glm::angleAxis(glm::radians(20.0f), glm::vec3(0, 1, 0));

    EXPECT_LT(glm::dot(q0, q1), 0.0f); // antipodal sign

    interp.AddSnapshot(1, 0.0, glm::vec3(0.0f), q0, glm::vec3(1.0f));
    interp.AddSnapshot(2, 0.1, glm::vec3(0.0f), q1, glm::vec3(1.0f));

    // Sample at midpoint renderTime = 0.05
    glm::vec3 pos, scale;
    glm::quat sampledRot;
    ASSERT_TRUE(interp.Sample(0.1, pos, sampledRot, scale));

    // The interpolated rotation should be approximately 15 degrees, NOT an inverted 195-degree spin
    float angleDeg = glm::degrees(2.0f * std::acos(std::min(1.0f, std::abs(sampledRot.w))));
    EXPECT_NEAR(angleDeg, 15.0f, 1.0f);
}

TEST(NetReplication, InterpolatorExtrapolationLimits)
{
    NetworkTransformInterpolator interp(0.05, 0.1); // 100ms max extrap

    interp.AddSnapshot(1, 0.0, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    interp.AddSnapshot(2, 0.1, glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

    // Velocity = 10 units / 0.1s = 100 units/s
    // 1. Extrapolate 50ms past latest (renderTime = 0.15, within maxExtrap 0.1s)
    glm::vec3 pos, scale;
    glm::quat rot;
    ASSERT_TRUE(interp.Sample(0.20, pos, rot, scale)); // renderTime = 0.20 - 0.05 = 0.15
    EXPECT_NEAR(pos.x, 15.0f, 0.1f); // 10 + 100 * 0.05 = 15

    // 2. Sample far in the future (renderTime = 1.0s, far exceeds 0.1s max extrapolation)
    // Should clamp safely to the last known position (10.0f) instead of runaway explosion
    ASSERT_TRUE(interp.Sample(1.05, pos, rot, scale)); // renderTime = 1.0s
    EXPECT_NEAR(pos.x, 10.0f, 0.01f);
}

TEST(NetReplication, EntityDespawnAndSceneCleanup)
{
    NetServerSystem::Config cfg;
    cfg.port = 8212;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    Scene serverScene;
    NetServerReplication serverRep(&serverScene, &server);

    Entity e = serverRep.SpawnEntity("DespawnTarget", glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(e.IsValid());
    uint64_t netId = e.GetComponent<NetworkIdentityComponent>().netId;

    EXPECT_TRUE(serverScene.FindEntityByNetId(netId).IsValid());

    // Despawn entity
    serverRep.DespawnEntity(netId);

    // Should be removed from server scene and net entity registry
    EXPECT_FALSE(serverScene.FindEntityByNetId(netId).IsValid());

    server.Stop();
}


TEST(NetReplication, EntityComponentSyncApplyAndExtract)
{
    Scene scene;
    Entity entity = SceneManager::CreateEntity(&scene, "TestMultiCompEntity", EntityType_Node);
    entity.AddComponent<NetworkIdentityComponent>(5001, 0, false);

    // Apply ComponentSyncMsg to dynamically add and configure components
    netproto::ComponentSyncMsg syncMsg;
    syncMsg.netId = 5001;
    syncMsg.compMask = NetComp_PointLight | NetComp_Sprite2D | NetComp_Text;

    syncMsg.pointLight.color = glm::vec4(0.1f, 0.7f, 0.9f, 1.0f);
    syncMsg.pointLight.intensity = 5.0f;
    syncMsg.pointLight.range = 18.0f;

    syncMsg.sprite2D.color = glm::vec4(1.0f, 0.0f, 0.5f, 1.0f);
    syncMsg.sprite2D.flipX = true;

    syncMsg.textComp.text = "Sync Extracted Test";

    scene.ApplyEntityComponentSync(entity, syncMsg);

    // Verify entity received components
    ASSERT_TRUE(entity.HasComponent<PointLightComponent>());
    ASSERT_TRUE(entity.HasComponent<Sprite2DComponent>());
    ASSERT_TRUE(entity.HasComponent<TextComponent>());

    const auto &plc = entity.GetComponent<PointLightComponent>();
    EXPECT_EQ(plc.color, syncMsg.pointLight.color);
    EXPECT_FLOAT_EQ(plc.intensity, syncMsg.pointLight.intensity);
    EXPECT_FLOAT_EQ(plc.range, syncMsg.pointLight.range);

    const auto &sc = entity.GetComponent<Sprite2DComponent>();
    EXPECT_EQ(sc.color, syncMsg.sprite2D.color);
    EXPECT_TRUE(sc.flipX);

    const auto &tc = entity.GetComponent<TextComponent>();
    EXPECT_EQ(tc.text, "Sync Extracted Test");

    // Now serialize entity components and verify roundtrip extraction
    netproto::ComponentSyncMsg extractedMsg;
    scene.SerializeEntityComponents(entity, NetComp_All, extractedMsg);

    EXPECT_EQ(extractedMsg.netId, 5001u);
    EXPECT_TRUE((extractedMsg.compMask & NetComp_PointLight) != 0);
    EXPECT_TRUE((extractedMsg.compMask & NetComp_Sprite2D) != 0);
    EXPECT_TRUE((extractedMsg.compMask & NetComp_Text) != 0);
    EXPECT_EQ(extractedMsg.pointLight.color, syncMsg.pointLight.color);
    EXPECT_EQ(extractedMsg.textComp.text, syncMsg.textComp.text);
}

TEST(NetReplication, LiveEditDirtyComponentBroadcast)
{
    NetServerSystem::Config cfg;
    cfg.port = 8213;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    Scene serverScene;
    NetServerReplication serverRep(&serverScene, &server);
    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &c) {
        serverRep.SyncWorldToClient(c.clientId);
    });

    Entity e = serverRep.SpawnEntity("LiveLight", glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(e.IsValid());
    uint64_t netId = e.GetComponent<NetworkIdentityComponent>().netId;

    e.AddComponent<PointLightComponent>();
    auto &plc = e.GetComponent<PointLightComponent>();
    plc.color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
    plc.intensity = 10.0f;

    auto &netComp = e.GetComponent<NetworkIdentityComponent>();
    netComp.MarkDirty(NetComp_PointLight);
    EXPECT_TRUE(netComp.IsDirty(NetComp_PointLight));

    // Client setup
    NetClientSystem client;
    Scene clientScene;
    NetClientReplication clientRep(&clientScene, &client);

    bool connected = false;
    client.onConnected = [&]() { connected = true; };
    client.Connect("127.0.0.1:8213", "LiveEditTester");

    // Pump events to complete connection and late-joiner sync
    for (int i = 0; i < 30 && !connected; i++)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(connected);

    // Give server time to send SpawnMsg + ComponentSyncMsg
    for (int i = 0; i < 20; i++)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Verify entity was spawned on client with initial component state
    Entity clientEnt = clientScene.FindEntityByNetId(netId);
    ASSERT_TRUE(clientEnt.IsValid());
    ASSERT_TRUE(clientEnt.HasComponent<PointLightComponent>());
    EXPECT_EQ(clientEnt.GetComponent<PointLightComponent>().color, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    // Live Edit: modify light color to Cyan on server
    plc.color = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f); // Cyan
    plc.intensity = 20.0f;
    netComp.MarkDirty(NetComp_PointLight);

    // Server broadcasts dirty components
    serverRep.BroadcastDirtyComponents();
    EXPECT_FALSE(netComp.IsDirty(NetComp_PointLight)); // Dirty cleared

    // Client receives update
    for (int i = 0; i < 20; i++)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Verify client received the live-edited color without respawning!
    EXPECT_EQ(clientEnt.GetComponent<PointLightComponent>().color, glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
    EXPECT_FLOAT_EQ(clientEnt.GetComponent<PointLightComponent>().intensity, 20.0f);

    client.Disconnect();
    server.Stop();
}
