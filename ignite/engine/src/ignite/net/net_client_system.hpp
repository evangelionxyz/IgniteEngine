// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_CLIENT_SYSTEM_HPP
#define IGN_NET_CLIENT_SYSTEM_HPP

#include "net_protocol.hpp"
#include <ignite/core/base.hpp>

#include <net/Client.hpp>
#include <net/BufferStream.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ignite
{
    // Client side of the game network layer. Game-thread only; network IO is
    // marshalled through Client::PumpEvents().
    class IGN_API NetClientSystem
    {
    public:
        enum class State : uint8_t
        {
            Disconnected,
            Connecting,   // socket connecting
            Handshaking,  // Hello sent, waiting for Welcome
            Connected,    // Welcome received
        };

        using LogCallback = std::function<void(const std::string &)>;

    public:
        NetClientSystem() = default;
        ~NetClientSystem();

        NetClientSystem(const NetClientSystem &) = delete;
        NetClientSystem &operator=(const NetClientSystem &) = delete;

        // address in "ip:port" form, e.g. "127.0.0.1:8192"
        void Connect(const std::string &address, const std::string &playerName);
        void Disconnect();

        // Dispatch network events and run keep-alive pings. Call once per frame.
        void Tick();

        State GetState() const { return m_State; }
        bool IsConnected() const { return m_State == State::Connected; }
        uint32_t GetClientId() const { return m_ClientId; }
        uint32_t GetServerTick() const { return m_ServerTick; }
        uint16_t GetTickRate() const { return m_TickRate; }
        uint16_t GetSnapshotRate() const { return m_SnapshotRate; }
        float GetRttMs() const { return m_RttMs; }
        bool GetRealTimeStatus(SteamNetConnectionRealTimeStatus_t *status) const;
        const std::string &GetDisconnectReason() const { return m_DisconnectReason; }

        void Send(const std::function<void(net::StreamWriter &)> &write, bool reliable = true);
        void SendChat(const std::string &text);

        // Hooks for the replication layer.
        std::function<void(const netproto::SpawnMsg &)> onSpawn;
        std::function<void(const netproto::DespawnMsg &)> onDespawn;
        std::function<void(const netproto::SnapshotMsg &)> onSnapshot;
        std::function<void(const netproto::ChatMsg &)> onChat;
        std::function<void()> onConnected;    // fired after a successful handshake
        std::function<void()> onDisconnected;

        void SetLogCallback(LogCallback cb) { m_Log = std::move(cb); }

    private:
        void OnConnectedEvent();
        void OnDisconnectedEvent();
        void OnData(net::Buffer buffer);
        uint32_t NowMs() const;
        void Log(const std::string &msg) const;

    private:
        std::unique_ptr<net::Client> m_Client;
        State m_State = State::Disconnected;
        bool m_DisconnectRequested = false; // set from event callbacks, handled after the pump
        std::string m_PlayerName;
        std::string m_DisconnectReason;

        uint32_t m_ClientId = 0;
        uint32_t m_ServerTick = 0;
        uint16_t m_TickRate = 60;
        uint16_t m_SnapshotRate = 20;
        float m_RttMs = 0.0f;

        std::chrono::steady_clock::time_point m_Epoch = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point m_LastPing{};
        std::vector<uint8_t> m_Scratch = std::vector<uint8_t>(64 * 1024);

        LogCallback m_Log;
    };
}

#endif
