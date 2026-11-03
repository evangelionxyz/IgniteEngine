// Copyright (c) 2026 Evangelion Manuhutu
// Google Test suite for NetworkManager Subsystem & Project Server Settings Integration:
// 1. NetworkManager singleton lifecycle & state transitions
// 2. StartServer() parameterless overload reading from ProjectInfo ServerSettings
// 3. Client connection, chat history buffering & client event callbacks
// 4. Real-time network diagnostics (GetStats)

#include <gtest/gtest.h>

#include <ignite/net/network_manager.hpp>
#include <ignite/project/project.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/net/net_client_system.hpp>

#include <chrono>
#include <thread>
#include <atomic>

using namespace ignite;

TEST(NetworkManager, LifecycleAndState)
{
    NetworkManager *net = NetworkManager::GetInstance();
    ASSERT_NE(net, nullptr);

    // Initial state: neither client nor server running
    EXPECT_FALSE(net->IsServerRunning());
    EXPECT_FALSE(net->IsConnected());
    EXPECT_FALSE(net->IsConnecting());
    EXPECT_EQ(net->GetClientId(), 0u);
    EXPECT_EQ(net->GetClientCount(), 0u);
}

TEST(NetworkManager, StartServerFromProjectSettings)
{
    NetworkManager *net = NetworkManager::GetInstance();
    ASSERT_NE(net, nullptr);

    // Configure project server settings
    ProjectInfo projInfo;
    projInfo.name = "NetProjectTest";
    projInfo.rootDirectory = std::filesystem::temp_directory_path() / "NetProjectTest";
    std::filesystem::create_directories(projInfo.rootDirectory);
    projInfo.serverSettings.port = 8221;
    projInfo.serverSettings.tickRate = 64;
    projInfo.serverSettings.snapshotRate = 32;
    projInfo.serverSettings.maxClients = 16;
    projInfo.serverSettings.autoStart = true;

    Ref<Project> project = Project::Create(projInfo);
    Project::SetActive(project);

    // Start server using project settings (no arguments)
    ASSERT_TRUE(net->StartServer());
    EXPECT_TRUE(net->IsServerRunning());
    EXPECT_EQ(net->GetClientCount(), 0u);

    // Stop server
    net->StopServer();
    EXPECT_FALSE(net->IsServerRunning());

    Project::SetActive(nullptr);
}

TEST(NetworkManager, ClientConnectAndChatRelay)
{
    NetworkManager *net = NetworkManager::GetInstance();
    ASSERT_NE(net, nullptr);

    std::atomic<bool> clientJoined = false;
    uint32_t joinedId = 0;
    net->SetClientJoinedCallback([&](uint32_t id, const std::string &name) {
        clientJoined = true;
        joinedId = id;
    });

    ASSERT_TRUE(net->StartServer(8222, 60, 20));
    EXPECT_TRUE(net->IsServerRunning());

    // Connect remote client
    NetClientSystem remoteClient;
    std::atomic<bool> remoteConnected = false;
    std::string receivedChatFromManager;

    remoteClient.onConnected = [&]() {
        remoteConnected = true;
    };
    remoteClient.onChat = [&](const netproto::ChatMsg &msg) {
        receivedChatFromManager = msg.text;
    };

    remoteClient.Connect("127.0.0.1:8222", "RemoteBob");

    auto start = std::chrono::steady_clock::now();
    while ((!remoteConnected || !clientJoined) &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        net->Update(0.016f);
        remoteClient.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(remoteConnected.load());
    ASSERT_TRUE(clientJoined.load());
    EXPECT_EQ(net->GetClientCount(), 1u);
    EXPECT_EQ(joinedId, remoteClient.GetClientId());

    // 1. Server sends chat -> remote client receives
    net->SendChat("Welcome to the server, Bob!");

    start = std::chrono::steady_clock::now();
    while (receivedChatFromManager.empty() &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        net->Update(0.016f);
        remoteClient.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_EQ(receivedChatFromManager, "Welcome to the server, Bob!");

    // 2. Remote client sends chat -> NetworkManager chat history records it
    remoteClient.SendChat("Thanks, glad to be here!");

    start = std::chrono::steady_clock::now();
    bool sawBobChat = false;
    while (!sawBobChat &&
           std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 2.0)
    {
        net->Update(0.016f);
        remoteClient.Tick();
        for (const auto &item : net->GetChatHistory())
        {
            if (item.text == "Thanks, glad to be here!")
            {
                sawBobChat = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_TRUE(sawBobChat);

    // Clean disconnect
    remoteClient.Disconnect();
    net->StopServer();
}

TEST(NetworkManager, RealTimeStatsReporting)
{
    NetworkManager *net = NetworkManager::GetInstance();
    ASSERT_NE(net, nullptr);

    NetStats stats = net->GetStats();
    EXPECT_GE(stats.rttMs, 0);
    EXPECT_GE(stats.packetLossPercent, 0.0f);
    EXPECT_GE(stats.inKBytesPerSec, 0.0f);
    EXPECT_GE(stats.outKBytesPerSec, 0.0f);
    EXPECT_EQ(stats.clientCount, 0u);
}

