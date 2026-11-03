// Copyright (c) 2026 Evangelion Manuhutu
// Google Test suite for NetServerSystem & NetClientSystem:
// 1. Loopback connection & handshake negotiation
// 2. Protocol version rejection & handshake timeout enforcement
// 3. Max clients capacity enforcement
// 4. Graceful disconnect & server kick with reason
// 5. Reliable chat messaging & rate limiting
// 6. Input sequence queuing & draining

#include <gtest/gtest.h>

#include <ignite/net/net_server_system.hpp>
#include <ignite/net/net_client_system.hpp>

#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

using namespace ignite;

namespace
{
    // Helper to step both server and client for a specified number of loopback iterations
    void StepNetwork(NetServerSystem &server, NetClientSystem &client, int iterations = 10, int sleepMs = 5)
    {
        for (int i = 0; i < iterations; ++i)
        {
            server.Tick();
            client.Tick();
            if (sleepMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }
        }
    }
}

TEST(NetSystem, LoopbackHandshakeConnection)
{
    NetServerSystem::Config cfg;
    cfg.port = 8201;
    cfg.tickRate = 60;
    cfg.snapshotRate = 20;

    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    std::atomic<bool> serverSawJoin = false;
    std::string joinedClientName;
    server.SetClientJoinedCallback([&](NetServerSystem::RemoteClient &c) {
        serverSawJoin = true;
        joinedClientName = c.name;
    });

    NetClientSystem client;
    std::atomic<bool> clientConnected = false;
    client.onConnected = [&]() {
        clientConnected = true;
    };

    client.Connect("127.0.0.1:8201", "AlicePlayer");

    // Pump until handshake finishes (timeout after 2 seconds)
    auto start = std::chrono::steady_clock::now();
    while ((!clientConnected || !serverSawJoin) &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(clientConnected.load());
    EXPECT_TRUE(serverSawJoin.load());
    EXPECT_EQ(joinedClientName, "AlicePlayer");
    EXPECT_TRUE(client.IsConnected());
    EXPECT_GT(client.GetClientId(), 0u);
    EXPECT_EQ(client.GetTickRate(), 60u);
    EXPECT_EQ(client.GetSnapshotRate(), 20u);
    EXPECT_EQ(server.GetClientCount(), 1u);
    EXPECT_TRUE(server.HasClient(client.GetClientId()));

    client.Disconnect();
    server.Stop();
}

TEST(NetSystem, VersionMismatchRejection)
{
    NetServerSystem::Config cfg;
    cfg.port = 8202;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    // Connect raw client that sends an invalid protocol version
    net::Client rawClient;
    rawClient.EnableEventQueue(true);
    std::string rejectReason;

    rawClient.SetDataReceivedCallback([&](const net::Buffer buf) {
        net::BufferStreamReader reader(buf);
        if (netproto::ReadMsgType(reader) == netproto::MsgType::Reject)
        {
            netproto::RejectMsg msg;
            if (msg.Read(reader))
            {
                rejectReason = msg.reason;
            }
        }
    });

    rawClient.ConnectToServer("127.0.0.1:8202");

    // Wait for TCP/UDP connection, then send bad version Hello
    auto start = std::chrono::steady_clock::now();
    bool sentHello = false;
    while (rejectReason.empty() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        rawClient.PumpEvents();

        if (!sentHello && rawClient.GetConnectionStatus() == net::Client::ConnectionStatus::Connected)
        {
            uint8_t storage[256];
            net::Buffer buf(storage, sizeof(storage));
            net::BufferStreamWriter writer(buf);
            netproto::HelloMsg badHello{ 9999, "TimeTraveler" }; // bad version
            badHello.Write(writer);
            rawClient.SendBuffer(writer.GetBuffer(), true);
            sentHello = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_FALSE(rejectReason.empty());
    EXPECT_NE(rejectReason.find("mismatch"), std::string::npos);

    rawClient.Disconnect();
    server.Stop();
}

TEST(NetSystem, HandshakeTimeoutDropsConnection)
{
    NetServerSystem::Config cfg;
    cfg.port = 8203;
    cfg.handshakeTimeoutSec = 0.2; // 200 ms timeout for fast test execution

    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    net::Client silentClient;
    silentClient.EnableEventQueue(true);
    silentClient.ConnectToServer("127.0.0.1:8203");

    // Pump until connected, but NEVER send HelloMsg
    auto start = std::chrono::steady_clock::now();
    while (silentClient.GetConnectionStatus() != net::Client::ConnectionStatus::Connected &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 1.0)
    {
        server.Tick();
        silentClient.PumpEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Advance time past the 0.2s timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    server.Tick();
    silentClient.PumpEvents();

    // Handshake never finished, so client count remains 0
    EXPECT_EQ(server.GetClientCount(), 0u);

    silentClient.Disconnect();
    server.Stop();
}

TEST(NetSystem, MaxClientsCapacityEnforced)
{
    NetServerSystem::Config cfg;
    cfg.port = 8204;
    cfg.maxClients = 1; // only 1 client allowed

    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    // 1. First client connects
    NetClientSystem client1;
    client1.Connect("127.0.0.1:8204", "FirstPlayer");

    auto start = std::chrono::steady_clock::now();
    while (!client1.IsConnected() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client1.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(client1.IsConnected());
    EXPECT_EQ(server.GetClientCount(), 1u);

    // 2. Second client attempts to connect
    NetClientSystem client2;
    client2.Connect("127.0.0.1:8204", "SecondPlayer");

    start = std::chrono::steady_clock::now();
    while (client2.GetState() != NetClientSystem::State::Disconnected &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client2.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Second client should be rejected because server is full
    EXPECT_FALSE(client2.IsConnected());
    EXPECT_EQ(server.GetClientCount(), 1u);

    client1.Disconnect();
    client2.Disconnect();
    server.Stop();
}

TEST(NetSystem, ClientGracefulDisconnect)
{
    NetServerSystem::Config cfg;
    cfg.port = 8205;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    std::atomic<bool> clientLeftFired = false;
    server.SetClientLeftCallback([&](NetServerSystem::RemoteClient &c) {
        clientLeftFired = true;
    });

    NetClientSystem client;
    client.Connect("127.0.0.1:8205", "DisconnectingPlayer");

    auto start = std::chrono::steady_clock::now();
    while (!client.IsConnected() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(client.IsConnected());
    EXPECT_EQ(server.GetClientCount(), 1u);

    // Client initiates graceful disconnect
    client.Disconnect();

    start = std::chrono::steady_clock::now();
    while (!clientLeftFired &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(clientLeftFired.load());
    EXPECT_EQ(server.GetClientCount(), 0u);

    server.Stop();
}

TEST(NetSystem, ServerKickWithReason)
{
    NetServerSystem::Config cfg;
    cfg.port = 8206;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    NetClientSystem client;
    client.Connect("127.0.0.1:8206", "KickedPlayer");

    auto start = std::chrono::steady_clock::now();
    while (!client.IsConnected() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(client.IsConnected());
    const uint32_t clientId = client.GetClientId();

    // Server kicks client
    server.Kick(clientId, "SpeedHack Detected");

    start = std::chrono::steady_clock::now();
    while (client.IsConnected() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_FALSE(client.IsConnected());
    EXPECT_NE(client.GetDisconnectReason().find("SpeedHack"), std::string::npos);

    client.Disconnect();
    server.Stop();
}

TEST(NetSystem, ReliableChatBroadcastAndRateLimiting)
{
    NetServerSystem::Config cfg;
    cfg.port = 8207;
    cfg.chatMinIntervalSec = 0.5; // 500ms limit

    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    NetClientSystem client1, client2;
    client1.Connect("127.0.0.1:8207", "ChatterA");
    client2.Connect("127.0.0.1:8207", "ChatterB");

    auto start = std::chrono::steady_clock::now();
    while ((!client1.IsConnected() || !client2.IsConnected()) &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(client1.IsConnected());
    ASSERT_TRUE(client2.IsConnected());

    std::vector<std::string> receivedChats;
    client2.onChat = [&](const netproto::ChatMsg &msg) {
        receivedChats.push_back(msg.text);
    };

    // Client 1 sends message 1
    client1.SendChat("Hello from Client 1");
    // Client 1 immediately spams message 2 (should be rate-limited by server)
    client1.SendChat("Spam message within rate limit");

    start = std::chrono::steady_clock::now();
    while (receivedChats.empty() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_FALSE(receivedChats.empty());
    EXPECT_EQ(receivedChats[0], "Hello from Client 1");
    // Rate limit dropped the second message
    EXPECT_EQ(receivedChats.size(), 1u);

    client1.Disconnect();
    client2.Disconnect();
    server.Stop();
}

TEST(NetSystem, InputQueueDrainingAndSequenceOrdering)
{
    NetServerSystem::Config cfg;
    cfg.port = 8208;
    NetServerSystem server(cfg);
    ASSERT_TRUE(server.Start());

    NetClientSystem client;
    client.Connect("127.0.0.1:8208", "PlayerInput");

    auto start = std::chrono::steady_clock::now();
    while (!client.IsConnected() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_TRUE(client.IsConnected());
    const uint32_t clientId = client.GetClientId();

    // Send sequence of inputs
    netproto::InputMsg msg;
    for (uint32_t seq = 1; seq <= 5; ++seq)
    {
        netproto::InputCmd cmd;
        cmd.seq = seq;
        cmd.move = glm::vec2(0.5f, 0.0f);
        cmd.yaw = 0.0f;
        cmd.buttons = 0;
        msg.cmds.push_back(cmd);
    }

    client.Send([&](net::StreamWriter &w) {
        msg.Write(w);
    }, false);

    start = std::chrono::steady_clock::now();
    std::vector<netproto::InputCmd> drained;
    while (drained.empty() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        server.Tick();
        client.Tick();
        drained = server.DrainInputs(clientId);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_EQ(drained.size(), 5u);
    for (size_t i = 0; i < drained.size(); ++i)
    {
        EXPECT_EQ(drained[i].seq, uint32_t(i + 1));
    }

    // A second drain should yield an empty list
    EXPECT_TRUE(server.DrainInputs(clientId).empty());

    client.Disconnect();
    server.Stop();
}

