// Copyright (c) 2026 Evangelion Manuhutu
// End-to-end integration test: Server authoritative replication, multiple clients,
// entity spawning, transform snapshots, interpolation, and despawn.

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cmath>

#include "ignite/net/net_server_system.hpp"
#include "ignite/net/net_client_system.hpp"
#include "ignite/net/net_replication.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene_manager.hpp"

using namespace ignite;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_failures++; } } while (0)

int main()
{
    std::printf("Running Replication & Multi-Client Integration Test...\n");

    // 1. Setup Server
    NetServerSystem::Config cfg;
    cfg.port = 8198;
    cfg.tickRate = 60;
    cfg.snapshotRate = 20;

    auto serverScene = Scene::Create(nullptr);
    NetServerSystem server(cfg);
    NetServerReplication serverRep(serverScene.get(), &server);

    uint32_t c1JoinedId = 0;
    uint32_t c2JoinedId = 0;

    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &c) {
        std::printf("[Server] Client joined: %s (id %u)\n", c.name.c_str(), c.clientId);
        if (c.name == "Player1") c1JoinedId = c.clientId;
        else if (c.name == "Player2") c2JoinedId = c.clientId;
        serverRep.SyncWorldToClient(c.clientId);
    });

    CHECK(server.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 2. Setup Client 1
    auto client1Scene = Scene::Create(nullptr);
    NetClientSystem client1;
    NetClientReplication client1Rep(client1Scene.get(), &client1);

    bool c1Connected = false;
    client1.onConnected = [&]() {
        c1Connected = true;
        std::printf("[Client 1] Connected! ClientId=%u\n", client1.GetClientId());
    };
    client1.Connect("127.0.0.1:8198", "Player1");

    // 3. Setup Client 2
    auto client2Scene = Scene::Create(nullptr);
    NetClientSystem client2;
    NetClientReplication client2Rep(client2Scene.get(), &client2);

    bool c2Connected = false;
    client2.onConnected = [&]() {
        c2Connected = true;
        std::printf("[Client 2] Connected! ClientId=%u\n", client2.GetClientId());
    };
    client2.Connect("127.0.0.1:8198", "Player2");

    // Pump until both connect
    auto start = std::chrono::steady_clock::now();
    while ((!c1Connected || !c2Connected || c1JoinedId == 0 || c2JoinedId == 0) &&
           std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < 3)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(c1Connected && c2Connected);
    CHECK(server.GetClientCount() == 2);
    std::printf("Both clients connected successfully.\n");

    // 4. Server spawns an entity owned by Client 1
    glm::vec3 spawnPos(10.0f, 2.0f, -5.0f);
    glm::quat spawnRot(1.0f, 0.0f, 0.0f, 0.0f);
    Entity srvEntity = serverRep.SpawnEntity("Player1_Avatar", spawnPos, spawnRot, c1JoinedId);
    CHECK(srvEntity.IsValid());
    uint64_t netId = srvEntity.GetComponent<NetworkIdentityComponent>().netId;
    CHECK(netId != 0);

    // Pump events to deliver spawn to clients
    for (int i = 0; i < 20; ++i)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        client1Rep.Update(0.016f);
        client2Rep.Update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify entity was replicated in both client scenes
    Entity c1Entity = client1Scene->FindEntityByNetId(netId);
    Entity c2Entity = client2Scene->FindEntityByNetId(netId);
    CHECK(c1Entity.IsValid());
    CHECK(c2Entity.IsValid());

    CHECK(c1Entity.GetComponent<NetworkIdentityComponent>().isLocalPlayer == true);
    CHECK(c2Entity.GetComponent<NetworkIdentityComponent>().isLocalPlayer == false);
    std::printf("Entity %llu spawned and replicated to both clients!\n", netId);

    // 5. Test transform replication and remote interpolation
    // Server moves entity to (25, 2, -5) over 1 second, sending periodic snapshots
    for (int tick = 0; tick < 60; ++tick)
    {
        // Server moves entity
        auto &tr = srvEntity.GetTransform();
        tr.local.translation.x += 0.25f; // moves from 10 to 25
        tr.world.translation = tr.local.translation;

        server.Tick();

        if (tick % 3 == 0) // 20 Hz snapshots
        {
            serverRep.BroadcastSnapshots();
        }

        client1.Tick();
        client2.Tick();
        client1Rep.Update(0.016f);
        client2Rep.Update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Give interpolator time to buffer and catch up
    for (int i = 0; i < 20; ++i)
    {
        client2.Tick();
        client2Rep.Update(0.02f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Check that Client 2 interpolated the entity towards the server's final position (25.0f)
    const glm::vec3 c2Pos = c2Entity.GetTransform().world.translation;
    std::printf("Server final pos.x: %.2f | Client 2 interpolated pos.x: %.2f\n",
                srvEntity.GetTransform().world.translation.x, c2Pos.x);
    CHECK(c2Pos.x > 15.0f); // Positively moved significantly towards target from 10.0f

    // 6. Test Despawn
    serverRep.DespawnEntity(netId);
    for (int i = 0; i < 20; ++i)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        client1Rep.Update(0.016f);
        client2Rep.Update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Entity c1EntityAfter = client1Scene->FindEntityByNetId(netId);
    Entity c2EntityAfter = client2Scene->FindEntityByNetId(netId);
    CHECK(!c1EntityAfter.IsValid());
    CHECK(!c2EntityAfter.IsValid());
    std::printf("Entity %llu successfully despawned on all clients!\n", netId);

    // 6.5 Test Pre-placed Scene Entity (avoid creating empty duplicate)
    // Create pre-placed entity in server scene with netId 2000
    Entity srvPreplaced = SceneManager::CreateEntity(serverScene.get(), "PreplacedMesh", EntityType_Mesh);
    srvPreplaced.AddComponent<NetworkIdentityComponent>(2000ULL, 0U, false, "");
    auto &srvTr = srvPreplaced.GetTransform();
    srvTr.local.translation = glm::vec3(50.0f, 0.0f, 50.0f);
    srvTr.world.translation = glm::vec3(50.0f, 0.0f, 50.0f);
    serverScene->RegisterNetEntity(2000ULL, srvPreplaced);

    // Create same pre-placed entity in client 1 scene (as loaded from level file)
    Entity c1Preplaced = SceneManager::CreateEntity(client1Scene.get(), "PreplacedMesh", EntityType_Mesh);
    c1Preplaced.AddComponent<NetworkIdentityComponent>(2000ULL, 0U, false, "");
    auto &c1Tr = c1Preplaced.GetTransform();
    c1Tr.local.translation = glm::vec3(0.0f, 0.0f, 0.0f);
    c1Tr.world.translation = glm::vec3(0.0f, 0.0f, 0.0f);
    client1Scene->RegisterNetEntity(2000ULL, c1Preplaced);

    // Sync world to client 1
    serverRep.SyncWorldToClient(c1JoinedId);

    for (int i = 0; i < 20; ++i)
    {
        server.Tick();
        client1.Tick();
        client1Rep.Update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify client 1 bound to c1Preplaced and did not duplicate
    size_t netCompCount = 0;
    for (auto e : client1Scene->registry->view<NetworkIdentityComponent>())
    {
        (void)e;
        netCompCount++;
    }
    CHECK(netCompCount == 1); // Only 2000 exists (netId was despawned in step 6)
    Entity c1Found = client1Scene->FindEntityByNetId(2000ULL);
    CHECK(c1Found.IsValid());
    CHECK(c1Found == c1Preplaced);
    CHECK(c1Found.GetTransform().world.translation.x == 50.0f);
    std::printf("Pre-placed scene entity test passed: correctly bound, no duplicates, transform snapped!\n");

    // 7. Cleanup
    client1.Disconnect();
    client2.Disconnect();
    for (int i = 0; i < 30; ++i)
    {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(server.GetClientCount() == 0);
    server.Stop();

    std::printf(g_failures == 0 ? "ALL REPLICATION TESTS PASSED!\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

