// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_SERVER_SYSTEM_HPP
#define IGN_NET_SERVER_SYSTEM_HPP

#include "net_protocol.hpp"
#include <ignite/core/base.hpp>

#include <net/Server.hpp>
#include <net/BufferStream.hpp>

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ignite
{
    // Authoritative server side of the game network layer.
    // All methods must be called from the game thread. Network IO runs on the
    // wrapper's thread and is marshalled through Server::PumpEvents().
    class IGN_API NetServerSystem
    {
    public:
        struct Config
        {
            int port = 8192;
            uint16_t tickRate = 60;
            uint16_t snapshotRate = 20;
            uint32_t maxClients = 32;
            double handshakeTimeoutSec = 5.0;
            double chatMinIntervalSec = 0.25;
        };

        struct RemoteClient
        {
            net::ClientID connection = 0;
            uint32_t clientId = 0;      // stable id handed out by the server (never 0)
            std::string name;
            bool handshaked = false;
            std::chrono::steady_clock::time_point connectedAt;
            std::chrono::steady_clock::time_point lastChatAt{};
            uint32_t lastInputSeq = 0;  // last accepted input sequence
            std::deque<netproto::InputCmd> pendingInputs;
        };

        using ClientCallback = std::function<void(RemoteClient &)>;
        using ChatCallback = std::function<void(RemoteClient &, const netproto::ChatMsg &)>;
        using LogCallback = std::function<void(const std::string &)>;

    public:
        explicit NetServerSystem(const Config &config);
        ~NetServerSystem();

        NetServerSystem(const NetServerSystem &) = delete;
        NetServerSystem &operator=(const NetServerSystem &) = delete;

        bool Start();
        void Stop();
        bool IsRunning() const { return m_Server && m_Server->IsRunning(); }

        // Advance one fixed tick: dispatch network events, enforce timeouts.
        void Tick();

        uint32_t GetTick() const { return m_Tick; }
        const Config &GetConfig() const { return m_Config; }

        // Client queries (only handshaked clients are visible).
        size_t GetClientCount() const;
        RemoteClient *FindClient(uint32_t clientId);
        bool HasClient(uint32_t clientId) const;
        std::vector<uint32_t> GetConnectedClientIds() const;
        std::vector<std::string> GetConnectedClientNames() const;
        bool GetClientRealTimeStatus(uint32_t clientId, SteamNetConnectionRealTimeStatus_t *status) const;
        template<typename Fn>
        void ForEachClient(Fn &&fn)
        {
            for (auto &[conn, client] : m_Clients)
            {
                if (client.handshaked)
                    fn(client);
            }
        }

        // Moves out the inputs queued for a client (oldest first).
        std::vector<netproto::InputCmd> DrainInputs(uint32_t clientId);

        // Messaging helpers (reliable by default).
        void SendTo(uint32_t clientId, const std::function<void(net::StreamWriter &)> &write, bool reliable = true);
        void Broadcast(const std::function<void(net::StreamWriter &)> &write, bool reliable = true, uint32_t excludeClientId = 0);
        void Kick(uint32_t clientId, const std::string &reason);

        void SetClientJoinedCallback(ClientCallback cb) { m_OnJoined = std::move(cb); }
        void SetClientLeftCallback(ClientCallback cb) { m_OnLeft = std::move(cb); }
        void SetLogCallback(LogCallback cb) { m_Log = std::move(cb); }
        void SetChatCallback(ChatCallback cb) { m_OnChat = std::move(cb); }

    private:
        void OnConnected(const net::ClientInfo &info);
        void OnDisconnected(const net::ClientInfo &info);
        void OnData(const net::ClientInfo &info, net::Buffer buffer);

        void HandleHello(RemoteClient &client, net::StreamReader &reader);
        void HandleInput(RemoteClient &client, net::StreamReader &reader);
        void HandleChat(RemoteClient &client, net::StreamReader &reader);
        void HandlePing(RemoteClient &client, net::StreamReader &reader);

        void Reject(net::ClientID connection, const std::string &reason);
        void Send(net::ClientID connection, const std::function<void(net::StreamWriter &)> &write, bool reliable);
        void Log(const std::string &msg) const;

    private:
        static constexpr size_t kMaxPendingInputs = 64;
        static constexpr size_t kMaxNameLength = 32;

        Config m_Config;
        std::unique_ptr<net::Server> m_Server;

        std::unordered_map<net::ClientID, RemoteClient> m_Clients;
        uint32_t m_NextClientId = 1;
        uint32_t m_Tick = 0;

        std::vector<uint8_t> m_Scratch;

        ClientCallback m_OnJoined;
        ClientCallback m_OnLeft;
        LogCallback m_Log;
        ChatCallback m_OnChat;
    };
}

#endif

