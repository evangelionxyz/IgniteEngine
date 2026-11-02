// Copyright (c) 2026 Evangelion Manuhutu
// Integration test: NetServerSystem + NetClientSystem over loopback

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

#include "ignite/net/net_server_system.hpp"
#include "ignite/net/net_client_system.hpp"

using namespace ignite;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); g_failures++; } } while (0)

int main()
{
    std::printf("Running NetServerSystem <-> NetClientSystem integration test...\n");

    NetServerSystem::Config cfg;
    cfg.port = 8196;
    cfg.tickRate = 60;
    cfg.snapshotRate = 20;

    NetServerSystem server(cfg);
    server.SetLogCallback([](const std::string &msg) {
        std::printf("[SERVER LOG] %s\n", msg.c_str());
    });

    uint32_t joinedClient = 0;
    uint32_t leftClient = 0;

    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &c) {
        std::printf("Server: Client joined: %s (id %u)\n", c.name.c_str(), c.clientId);
        joinedClient = c.clientId;
    });

    server.SetClientLeftCallback([&](NetServerSystem::RemoteClient &c) {
        std::printf("Server: Client left: %s (id %u)\n", c.name.c_str(), c.clientId);
        leftClient = c.clientId;
    });

    CHECK(server.Start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetClientSystem client;
    client.SetLogCallback([](const std::string &msg) {
        std::printf("[CLIENT LOG] %s\n", msg.c_str());
    });

    bool clientConnected = false;
    client.onConnected = [&]() {
        std::printf("Client: onConnected fired! ClientId=%u\n", client.GetClientId());
        clientConnected = true;
    };

    client.Connect("127.0.0.1:8196", "TestPlayer");

    // Pump both for up to 3 seconds
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

    // Pump to receive input
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

    // Test graceful client disconnect
    client.Disconnect();
    for (int i = 0; i < 50; i++)
    {
        server.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CHECK(leftClient == joinedClient);
    CHECK(server.GetClientCount() == 0);

    server.Stop();

    std::printf(g_failures == 0 ? "INTEGRATION TEST PASSED\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
