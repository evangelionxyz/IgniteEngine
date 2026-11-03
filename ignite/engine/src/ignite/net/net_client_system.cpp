// Copyright (c) 2026 Evangelion Manuhutu

#include "net_client_system.hpp"

namespace ignite
{
    NetClientSystem::~NetClientSystem()
    {
        Disconnect();
    }

    void NetClientSystem::Connect(const std::string &address, const std::string &playerName)
    {
        Disconnect();

        m_PlayerName = playerName;
        m_DisconnectReason.clear();
        m_ClientId = 0;
        m_RttMs = 0.0f;

        m_Client = std::make_unique<net::Client>();
        m_Client->EnableEventQueue(true);
        m_Client->SetServerConnectedCallback([this]() { OnConnectedEvent(); });
        m_Client->SetServerDisconnectedCallback([this]() { OnDisconnectedEvent(); });
        m_Client->SetDataReceivedCallback([this](const net::Buffer buffer) { OnData(buffer); });

        m_State = State::Connecting;
        Log("Connecting to " + address);
        m_Client->ConnectToServer(address);
    }

    void NetClientSystem::Disconnect()
    {
        if (!m_Client)
            return;

        const bool wasActive = m_State != State::Disconnected;
        m_Client->Disconnect();
        m_Client.reset();
        m_State = State::Disconnected;
        m_DisconnectRequested = false;

        if (wasActive && onDisconnected)
            onDisconnected();
    }

    void NetClientSystem::Tick()
    {
        if (!m_Client)
            return;

        m_Client->PumpEvents();

        // Callbacks run inside PumpEvents, so teardown is deferred until it returns.
        if (m_DisconnectRequested)
        {
            Disconnect();
            return;
        }

        // Socket failed before/while connecting (no Disconnected event is raised in that case).
        if (m_State == State::Connecting
            && m_Client->GetConnectionStatus() == net::Client::ConnectionStatus::FailedToConnect)
        {
            m_DisconnectReason = m_Client->GetConnectionDebugMessage();
            Log("Failed to connect: " + m_DisconnectReason);
            Disconnect();
            return;
        }

        if (m_State == State::Connected)
        {
            const auto now = std::chrono::steady_clock::now();
            if (now - m_LastPing >= std::chrono::seconds(1))
            {
                m_LastPing = now;
                netproto::PingMsg ping;
                ping.clientTimeMs = NowMs();
                Send([&](net::StreamWriter &w) { ping.Write(w); }, false);
            }
        }
    }

    void NetClientSystem::Send(const std::function<void(net::StreamWriter &)> &write, bool reliable)
    {
        if (!m_Client)
            return;

        net::BufferStreamWriter writer(net::Buffer(m_Scratch.data(), m_Scratch.size()));
        write(writer);
        m_Client->SendBuffer(writer.GetBuffer(), reliable);
    }

    void NetClientSystem::SendChat(const std::string &text)
    {
        if (!IsConnected())
            return;

        netproto::ChatMsg msg;
        msg.text = text;
        Send([&](net::StreamWriter &w) { msg.Write(w); }, true);
    }

    void NetClientSystem::OnConnectedEvent()
    {
        m_State = State::Handshaking;

        netproto::HelloMsg hello;
        hello.name = m_PlayerName;
        Send([&](net::StreamWriter &w) { hello.Write(w); }, true);
    }

    void NetClientSystem::OnDisconnectedEvent()
    {
        if (m_State == State::Disconnected)
            return;

        if (m_DisconnectReason.empty() && m_Client)
            m_DisconnectReason = m_Client->GetConnectionDebugMessage();

        Log("Disconnected: " + m_DisconnectReason);
        m_DisconnectRequested = true;
    }

    void NetClientSystem::OnData(net::Buffer buffer)
    {
        net::BufferStreamReader reader(buffer);
        const netproto::MsgType type = netproto::ReadMsgType(reader);

        // Before Welcome, only Welcome/Reject are meaningful.
        if (m_State == State::Handshaking
            && type != netproto::MsgType::Welcome && type != netproto::MsgType::Reject)
            return;

        switch (type)
        {
        case netproto::MsgType::Welcome:
        {
            netproto::WelcomeMsg msg;
            if (m_State != State::Handshaking || !msg.Read(reader))
                break;

            m_ClientId = msg.clientId;
            m_ServerTick = msg.serverTick;
            m_TickRate = msg.tickRate;
            m_SnapshotRate = msg.snapshotRate;
            m_State = State::Connected;
            Log("Joined as client " + std::to_string(m_ClientId));
            if (onConnected)
                onConnected();
            break;
        }
        case netproto::MsgType::Reject:
        {
            netproto::RejectMsg msg;
            if (msg.Read(reader))
                m_DisconnectReason = msg.reason;
            Log("Rejected: " + m_DisconnectReason);
            break; // the server closes the connection right after
        }
        case netproto::MsgType::Spawn:
        {
            netproto::SpawnMsg msg;
            if (msg.Read(reader) && onSpawn)
                onSpawn(msg);
            break;
        }
        case netproto::MsgType::Despawn:
        {
            netproto::DespawnMsg msg;
            if (msg.Read(reader) && onDespawn)
                onDespawn(msg);
            break;
        }
        case netproto::MsgType::ComponentSync:
        {
            netproto::ComponentSyncMsg msg;
            if (msg.Read(reader) && onComponentSync)
                onComponentSync(msg);
            break;
        }
        case netproto::MsgType::Snapshot:
        {
            netproto::SnapshotMsg msg;
            if (!msg.Read(reader))
                break;

            if (msg.serverTick > m_ServerTick)
                m_ServerTick = msg.serverTick;
            if (onSnapshot)
                onSnapshot(msg);
            break;
        }
        case netproto::MsgType::Pong:
        {
            netproto::PingMsg msg;
            if (!msg.Read(reader))
                break;

            const float sample = static_cast<float>(NowMs() - msg.clientTimeMs);
            m_RttMs = m_RttMs == 0.0f ? sample : m_RttMs * 0.8f + sample * 0.2f;
            break;
        }
        case netproto::MsgType::Chat:
        {
            netproto::ChatMsg msg;
            if (msg.Read(reader) && onChat)
                onChat(msg);
            break;
        }
        default:
            break;
        }
    }

    uint32_t NetClientSystem::NowMs() const
    {
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_Epoch).count());
    }

    void NetClientSystem::Log(const std::string &msg) const
    {
        if (m_Log)
            m_Log("[NetClient] " + msg);
    }

    bool NetClientSystem::GetRealTimeStatus(SteamNetConnectionRealTimeStatus_t *status) const
    {
        if (m_Client)
            return m_Client->GetRealTimeStatus(status);
        return false;
    }
}
