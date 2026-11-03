// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NETWORK_MANAGER_HPP
#define IGN_NETWORK_MANAGER_HPP

#include <ignite/core/base.hpp>
#include <ignite/core/subsystem.hpp>
#include <ignite/project/project.hpp>
#include "net_protocol.hpp"
#include "net_client_system.hpp"
#include "net_server_system.hpp"
#include "net_replication.hpp"

#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace ignite
{
    class Scene;

    struct NetStats
    {
        int rttMs = 0;
        float packetLossPercent = 0.0f;
        float inKBytesPerSec = 0.0f;
        float outKBytesPerSec = 0.0f;
        uint32_t clientCount = 0;
    };

    struct NetChatMessage
    {
        uint32_t senderId = 0;
        std::string senderName;
        std::string text;
    };

    class IGN_API NetworkManager : public Subsystem
    {
    public:
        using ChatCallback = std::function<void(const NetChatMessage &)>;

    public:
        NetworkManager();
        virtual ~NetworkManager() override;

        virtual void Init() override;
        virtual void Shutdown() override;

        static NetworkManager *GetInstance();

        // ---------------------------------------------------------------------
        // Scene Association
        // ---------------------------------------------------------------------
        void SetActiveScene(Scene *scene);
        Scene *GetActiveScene() const { return m_ActiveScene; }

        // ---------------------------------------------------------------------
        // Client Mode
        // ---------------------------------------------------------------------
        bool Connect(const std::string &address, uint16_t port, const std::string &playerName = "Player");
        void Disconnect();
        bool IsConnected() const;
        bool IsConnecting() const;
        uint32_t GetClientId() const;

        // ---------------------------------------------------------------------
        // Server Mode (Host / Dedicated)
        // ---------------------------------------------------------------------
        bool StartServer();
        bool StartServer(const NetServerSystem::Config &config);
        bool StartServer(uint16_t port, uint16_t tickRate = 60, uint16_t snapshotRate = 20);
        void StopServer();
        bool IsServerRunning() const;

        void KickClient(uint32_t clientId, const std::string &reason = "");
        size_t GetClientCount() const;
        std::vector<uint32_t> GetConnectedClientIds() const;
        std::vector<std::string> GetConnectedClientNames() const;
        uint32_t GetServerTick() const;
        uint32_t GetClientServerTick() const;

        using ClientEventCallback = std::function<void(uint32_t clientId, const std::string &name)>;
        void SetClientJoinedCallback(ClientEventCallback cb) { m_ClientJoinedCallback = std::move(cb); }
        void SetClientLeftCallback(ClientEventCallback cb) { m_ClientLeftCallback = std::move(cb); }

        // ---------------------------------------------------------------------
        // Per-Frame Update
        // ---------------------------------------------------------------------
        void Update(float deltaTime);

        // ---------------------------------------------------------------------
        // Input & Actions
        // ---------------------------------------------------------------------
        void SendInput(const glm::vec2 &move, uint16_t buttons = 0, float yaw = 0.0f);
        void SendInput(const netproto::InputCmd &cmd);

        // ---------------------------------------------------------------------
        // Chat
        // ---------------------------------------------------------------------
        void SendChat(const std::string &text);
        void SetChatCallback(ChatCallback cb) { m_ChatCallback = std::move(cb); }
        const std::vector<NetChatMessage> &GetChatHistory() const { return m_ChatHistory; }

        // ---------------------------------------------------------------------
        // Network Simulation (Dev / QA)
        // ---------------------------------------------------------------------
        void SetSimulationSettings(int lagMs, float lossPercent);
        int GetSimulationLagMs() const { return m_SimLagMs; }
        float GetSimulationPacketLoss() const { return m_SimPacketLoss; }

        // ---------------------------------------------------------------------
        // ---------------------------------------------------------------------
        // Configuration & Project Synchronization
        // ---------------------------------------------------------------------
        void SyncFromProject(Project *project = nullptr);
        void SyncToProject(Project *project = nullptr);

        ServerSettings GetServerSettings() const;
        void SetServerSettings(const ServerSettings &settings);

        uint16_t GetPort() const { return static_cast<uint16_t>(m_UiPort); }
        void SetPort(uint16_t port);

        // ---------------------------------------------------------------------
        // Diagnostics & ImGui UI
        // ---------------------------------------------------------------------
        NetStats GetStats() const;
        void OnImGuiRender(bool *pOpen = nullptr);

        // Direct system access
        NetClientSystem *GetClientSystem() { return m_ClientSystem.get(); }
        NetClientReplication *GetClientReplication() { return m_ClientReplication.get(); }
        NetServerSystem *GetServerSystem() { return m_ServerSystem.get(); }
        NetServerReplication *GetServerReplication() { return m_ServerReplication.get(); }

    private:
        static NetworkManager *s_Instance;

        Scene *m_ActiveScene = nullptr;

        std::unique_ptr<NetClientSystem> m_ClientSystem;
        std::unique_ptr<NetClientReplication> m_ClientReplication;

        std::unique_ptr<NetServerSystem> m_ServerSystem;
        std::unique_ptr<NetServerReplication> m_ServerReplication;

        int m_SimLagMs = 0;
        float m_SimPacketLoss = 0.0f;

        uint32_t m_NextInputSeq = 1;
        ChatCallback m_ChatCallback;
        ClientEventCallback m_ClientJoinedCallback;
        ClientEventCallback m_ClientLeftCallback;
        std::vector<NetChatMessage> m_ChatHistory;
        std::vector<netproto::InputCmd> m_InputHistory;

        // Server tick accumulation
        float m_ServerTickAccumulator = 0.0f;
        float m_ServerSnapshotAccumulator = 0.0f;

        // UI & Network configuration state buffers
        char m_UiAddressBuf[128] = "127.0.0.1";
        int m_UiPort = 8192;
        int m_UiTickRate = 60;
        int m_UiSnapshotRate = 20;
        int m_UiMaxClients = 32;
        float m_UiHandshakeTimeout = 5.0f;
        float m_UiChatInterval = 0.25f;
        bool m_UiAutoStart = false;
        char m_UiPlayerName[64] = "Player";
        char m_UiChatInputBuf[256] = "";

        Ref<Project> m_LastSyncedProject;
        ServerSettings m_LastSyncedServerSettings;
    };
}

#endif

