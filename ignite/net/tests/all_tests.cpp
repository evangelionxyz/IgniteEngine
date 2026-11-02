// Copyright (c) 2026 Evangelion Manuhutu
// Comprehensive Network Infrastructure Test Suite:
// 1. Protocol serialization & quantization round-trips
// 2. Client-Server system connection, input handling, and disconnects
// 3. Entity replication, authoritative movement, interpolation, and despawning

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>

#include "ignite/net/net_protocol.hpp"
#include "ignite/net/net_server_system.hpp"
#include "ignite/net/net_client_system.hpp"
#include "ignite/net/net_replication.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include <net/BufferStream.hpp>

using namespace ignite;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_failures++; } } while (0)

static float QuatAngleDeg(const glm::quat &a, const glm::quat &b)
{
    float d = std::min(1.0f, std::abs(glm::dot(a, b)));
    return glm::degrees(2.0f * std::acos(d));
}

void RunProtocolTests()
{
    std::printf("\n=== [1/3] Running Protocol Tests ===\n");
    uint8_t storage[64 * 1024];
    net::Buffer buf(storage, sizeof(storage));

    // Quaternion quantization error
    {
        float worst = 0.0f;
        for (int i = 0; i < 2000; i++)
        {
            glm::vec3 axis = glm::normalize(glm::vec3(std::sin(i * 1.3f) + 0.1f, std::cos(i * 0.7f), std::sin(i * 2.1f) - 0.2f));
            glm::quat q = glm::angleAxis(i * 0.173f, axis);
            glm::quat r = netproto::DequantizeQuat(netproto::QuantizeQuat(q));
            worst = std::max(worst, QuatAngleDeg(q, r));
        }
        std::printf("  Worst quat quantization error: %.4f deg\n", worst);
        CHECK(worst < 0.5f);
    }

    // Position quantization
    {
        for (float v : { -1000.0f, -0.25f, 0.0f, 3.14159f, 999.999f })
            CHECK(std::abs(netproto::DequantizePos(netproto::QuantizePos(v)) - v) < 0.001f);
        CHECK(std::abs(netproto::DequantizePos(netproto::QuantizePos(1e9f)) - netproto::POS_RANGE) < 0.01f);
    }

    // Snapshot round trip
    {
        netproto::SnapshotMsg in;
        in.serverTick = 1234;
        in.lastProcessedInputSeq = 99;
        for (uint64_t i = 0; i < 50; i++)
        {
            netproto::EntityState e;
            e.netId = 1000 + i;
            e.position = { float(i), -float(i) * 0.5f, 7.25f };
            e.rotation = glm::angleAxis(0.1f * float(i), glm::vec3(0, 1, 0));
            e.scale = { 1, 2, 3 };
            in.entities.push_back(e);
        }

        net::BufferStreamWriter writer(buf);
        in.Write(writer);

        netproto::SnapshotMsg out;
        net::BufferStreamReader reader(buf);
        netproto::MsgType type;
        CHECK(reader.ReadRaw(type) && type == netproto::MsgType::Snapshot);
        CHECK(out.Read(reader));
        CHECK(out.serverTick == in.serverTick);
        CHECK(out.lastProcessedInputSeq == in.lastProcessedInputSeq);
        CHECK(out.entities.size() == in.entities.size());
        for (size_t i = 0; i < in.entities.size(); i++)
        {
            CHECK(out.entities[i].netId == in.entities[i].netId);
            CHECK(glm::distance(out.entities[i].position, in.entities[i].position) < 0.005f);
            CHECK(QuatAngleDeg(out.entities[i].rotation, in.entities[i].rotation) < 0.5f);
            CHECK(glm::distance(out.entities[i].scale, in.entities[i].scale) < 0.01f);
        }
    }

    // Handshake and input round trip
    {
        netproto::HelloMsg hIn{ netproto::PROTOCOL_VERSION, "PlayerAlice" };
        net::BufferStreamWriter w(buf);
        hIn.Write(w);
        netproto::HelloMsg hOut;
        net::BufferStreamReader r(buf);
        netproto::MsgType t;
        CHECK(r.ReadRaw(t) && t == netproto::MsgType::Hello);
        CHECK(hOut.Read(r));
        CHECK(hOut.version == hIn.version);
        CHECK(hOut.name == hIn.name);
    }

    std::printf("  Protocol tests complete. Current failures: %d\n", g_failures);
}

void RunClientServerSystemTests()
{
    std::printf("\n=== [2/3] Running Client-Server System Loopback Tests ===\n");
    NetServerSystem::Config cfg;
    cfg.port = 8196;
    cfg.tickRate = 60;
    cfg.snapshotRate = 20;

    NetServerSystem server(cfg);
    uint32_t joinedClient = 0;
    uint32_t leftClient = 0;

    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &c) {
        std::printf("  Server: Client joined: %s (id %u)\n", c.name.c_str(), c.clientId);
        joinedClient = c.clientId;
    });

    server.SetClientLeftCallback([&](NetServerSystem::RemoteClient &c) {
        std::printf("  Server: Client left: %s (id %u)\n", c.name.c_str(), c.clientId);
        leftClient = c.clientId;
    });

    CHECK(server.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    NetClientSystem client;
    bool clientConnected = false;
    client.onConnected = [&]() {
        clientConnected = true;
    };

    client.Connect("127.0.0.1:8196", "SystemTestPlayer");

    auto start = std::chrono::steady_clock::now();
    while ((!clientConnected || joinedClient == 0) &&
           std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < 3)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(clientConnected);
    CHECK(joinedClient != 0);
    CHECK(client.GetClientId() == joinedClient);
    CHECK(server.GetClientCount() == 1);

    // Send input from client
    client.Send([&](net::StreamWriter &w) {
        netproto::InputMsg in;
        in.cmds.push_back({ 1, { 0.5f, -0.5f }, 1.2f, netproto::InputButton_Jump });
        in.Write(w);
    }, false);

    for (int i = 0; i < 20; i++)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto inputs = server.DrainInputs(joinedClient);
    CHECK(!inputs.empty());
    if (!inputs.empty())
    {
        CHECK(inputs[0].seq == 1);
        CHECK(inputs[0].buttons == netproto::InputButton_Jump);
        CHECK(std::abs(inputs[0].move.x - 0.5f) < 0.05f);
    }

    client.Disconnect();
    for (int i = 0; i < 50; i++)
    {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(leftClient == joinedClient);
    CHECK(server.GetClientCount() == 0);
    server.Stop();

    std::printf("  Client-server system tests complete. Current failures: %d\n", g_failures);
}

void RunReplicationTests()
{
    std::printf("\n=== [3/3] Running Replication & Multi-Client Tests ===\n");
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
        std::printf("  [Server] Client joined: %s (id %u)\n", c.name.c_str(), c.clientId);
        if (c.name == "Player1") c1JoinedId = c.clientId;
        else if (c.name == "Player2") c2JoinedId = c.clientId;
        serverRep.SyncWorldToClient(c.clientId);
    });

    CHECK(server.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto client1Scene = Scene::Create(nullptr);
    NetClientSystem client1;
    NetClientReplication client1Rep(client1Scene.get(), &client1);
    bool c1Connected = false;
    client1.onConnected = [&]() { c1Connected = true; };
    client1.Connect("127.0.0.1:8198", "Player1");

    auto client2Scene = Scene::Create(nullptr);
    NetClientSystem client2;
    NetClientReplication client2Rep(client2Scene.get(), &client2);
    bool c2Connected = false;
    client2.onConnected = [&]() { c2Connected = true; };
    client2.Connect("127.0.0.1:8198", "Player2");

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

    // Server spawns an entity owned by Client 1
    glm::vec3 spawnPos(10.0f, 2.0f, -5.0f);
    glm::quat spawnRot(1.0f, 0.0f, 0.0f, 0.0f);
    Entity srvEntity = serverRep.SpawnEntity("Player1_Avatar", spawnPos, spawnRot, c1JoinedId);
    CHECK(srvEntity.IsValid());
    uint64_t netId = srvEntity.GetComponent<NetworkIdentityComponent>().netId;
    CHECK(netId != 0);

    // Pump to deliver spawn
    for (int i = 0; i < 20; ++i)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        client1Rep.Update(0.016f);
        client2Rep.Update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Entity c1Entity = client1Scene->FindEntityByNetId(netId);
    Entity c2Entity = client2Scene->FindEntityByNetId(netId);
    CHECK(c1Entity.IsValid());
    CHECK(c2Entity.IsValid());
    CHECK(c1Entity.GetComponent<NetworkIdentityComponent>().isLocalPlayer == true);
    CHECK(c2Entity.GetComponent<NetworkIdentityComponent>().isLocalPlayer == false);
    std::printf("  Spawned netId %llu: Client 1 (owner, local=true), Client 2 (remote, local=false)\n", netId);

    // Authoritative entity movement over 60 ticks (1 sec)
    for (int tick = 0; tick < 60; ++tick)
    {
        auto &tr = srvEntity.GetTransform();
        tr.local.translation.x += 0.25f; // 10 -> 25
        tr.world.translation = tr.local.translation;

        server.Tick();
        if (tick % 3 == 0)
        {
            serverRep.BroadcastSnapshots();
        }

        client1.Tick();
        client2.Tick();
        client1Rep.Update(0.016f);
        client2Rep.Update(0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int i = 0; i < 20; ++i)
    {
        client2.Tick();
        client2Rep.Update(0.02f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    const glm::vec3 c2Pos = c2Entity.GetTransform().world.translation;
    std::printf("  Final Server pos.x: %.2f | Client 2 interpolated pos.x: %.2f\n",
                srvEntity.GetTransform().world.translation.x, c2Pos.x);
    CHECK(c2Pos.x > 15.0f);

    // Despawn
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

    CHECK(!client1Scene->FindEntityByNetId(netId).IsValid());
    CHECK(!client2Scene->FindEntityByNetId(netId).IsValid());
    std::printf("  Despawned netId %llu successfully\n", netId);

    client1.Disconnect();
    client2.Disconnect();
    for (int i = 0; i < 30; ++i)
    {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(server.GetClientCount() == 0);
    server.Stop();

    std::printf("  Replication tests complete. Current failures: %d\n", g_failures);
}

int main()
{
    std::printf("============================================================\n");
    std::printf("    IGNITE GAME NETWORKING - FULL VERIFICATION SUITE       \n");
    std::printf("============================================================\n");

    RunProtocolTests();
    RunClientServerSystemTests();
    RunReplicationTests();

    std::printf("\n============================================================\n");
    if (g_failures == 0)
    {
        std::printf("   >>> ALL TESTS PASSED! INFRASTRUCTURE READY <<<\n");
    }
    else
    {
        std::printf("   >>> TOTAL FAILURES: %d <<<\n", g_failures);
    }
    std::printf("============================================================\n");

    return g_failures == 0 ? 0 : 1;
}

