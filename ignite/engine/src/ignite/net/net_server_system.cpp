// Copyright (c) 2026 Evangelion Manuhutu

#include "net_server_system.hpp"

#include <algorithm>
#include <cctype>

namespace ignite
{
    namespace
    {
        // Keep printable ASCII only and bound the length so names are safe to log/display.
        std::string SanitizeName(const std::string &in, size_t maxLen)
        {
            std::string out;
            out.reserve(std::min(in.size(), maxLen));
            for (unsigned char c : in)
            {
                if (out.size() >= maxLen)
                    break;
                if (c >= 0x20 && c < 0x7F)
                    out.push_back(static_cast<char>(c));
            }
            return out;
        }
    }

    NetServerSystem::NetServerSystem(const Config &config)
        : m_Config(config)
    {
        m_Scratch.resize(256 * 1024);
    }

    NetServerSystem::~NetServerSystem()
    {
        Stop();
    }

    bool NetServerSystem::Start()
    {
        if (m_Server)
            return IsRunning();

        m_Server = std::make_unique<net::Server>(m_Config.port);
        m_Server->EnableEventQueue(true);
        m_Server->SetClientConnectedCallback([this](const net::ClientInfo &info) { OnConnected(info); });
        m_Server->SetClientDisconnectedCallback([this](const net::ClientInfo &info) { OnDisconnected(info); });
        m_Server->SetDataReceivedCallback([this](const net::ClientInfo &info, const net::Buffer buffer) { OnData(info, buffer); });
        m_Server->Start();

        Log("Listening on port " + std::to_string(m_Config.port));
        return true;
    }

    void NetServerSystem::Stop()
    {
        if (!m_Server)
            return;

        m_Server->Stop();
        m_Server.reset();
        m_Clients.clear();
    }

    void NetServerSystem::Tick()
    {
        if (!m_Server)
            return;

        m_Server->PumpEvents();
        ++m_Tick;

        // Drop connections that never completed the handshake.
        const auto now = std::chrono::steady_clock::now();
        std::vector<net::ClientID> timedOut;
        for (const auto &[conn, client] : m_Clients)
        {
            if (client.handshaked)
                continue;

            const double age = std::chrono::duration<double>(now - client.connectedAt).count();
            if (age > m_Config.handshakeTimeoutSec)
                timedOut.push_back(conn);
        }

        for (net::ClientID conn : timedOut)
        {
            Log("Handshake timeout for connection " + std::to_string(conn));
            // Kick removes the client; the Disconnected event is delivered on the next pump.
            m_Server->KickClient(conn);
        }
    }

    size_t NetServerSystem::GetClientCount() const
    {
        size_t count = 0;
        for (const auto &[conn, client] : m_Clients)
        {
            if (client.handshaked)
                ++count;
        }
        return count;
    }

    NetServerSystem::RemoteClient *NetServerSystem::FindClient(uint32_t clientId)
    {
        for (auto &[conn, client] : m_Clients)
        {
            if (client.handshaked && client.clientId == clientId)
                return &client;
        }
        return nullptr;
    }

    std::vector<netproto::InputCmd> NetServerSystem::DrainInputs(uint32_t clientId)
    {
        std::vector<netproto::InputCmd> out;
        if (RemoteClient *client = FindClient(clientId))
        {
            out.assign(client->pendingInputs.begin(), client->pendingInputs.end());
            client->pendingInputs.clear();
        }
        return out;
    }

    void NetServerSystem::Send(net::ClientID connection, const std::function<void(net::StreamWriter &)> &write, bool reliable)
    {
        if (!m_Server)
            return;

        net::BufferStreamWriter writer(net::Buffer(m_Scratch.data(), m_Scratch.size()));
        write(writer);
        m_Server->SendBufferToClient(connection, writer.GetBuffer(), reliable);
    }

    void NetServerSystem::SendTo(uint32_t clientId, const std::function<void(net::StreamWriter &)> &write, bool reliable)
    {
        if (RemoteClient *client = FindClient(clientId))
            Send(client->connection, write, reliable);
    }

    void NetServerSystem::Broadcast(const std::function<void(net::StreamWriter &)> &write, bool reliable, uint32_t excludeClientId)
    {
        if (!m_Server)
            return;

        net::BufferStreamWriter writer(net::Buffer(m_Scratch.data(), m_Scratch.size()));
        write(writer);
        const net::Buffer buffer = writer.GetBuffer();

        for (const auto &[conn, client] : m_Clients)
        {
            if (client.handshaked && client.clientId != excludeClientId)
                m_Server->SendBufferToClient(conn, buffer, reliable);
        }
    }

    void NetServerSystem::Kick(uint32_t clientId, const std::string &reason)
    {
        if (RemoteClient *client = FindClient(clientId))
        {
            Log("Kicking client " + std::to_string(clientId) + ": " + reason);
            const net::ClientID conn = client->connection;
            Send(conn, [&](net::StreamWriter &w) { netproto::RejectMsg{ reason }.Write(w); }, true);
            m_Server->KickClient(conn);
        }
    }

    void NetServerSystem::Reject(net::ClientID connection, const std::string &reason)
    {
        Log("Rejecting connection " + std::to_string(connection) + ": " + reason);
        Send(connection, [&](net::StreamWriter &w) { netproto::RejectMsg{ reason }.Write(w); }, true);
    }

    void NetServerSystem::OnConnected(const net::ClientInfo &info)
    {
        RemoteClient &client = m_Clients[info.ID];
        client.connection = info.ID;
        client.connectedAt = std::chrono::steady_clock::now();
        Log("Connection opened: " + std::to_string(info.ID));

        if (m_Clients.size() > m_Config.maxClients)
        {
            Reject(info.ID, "Server full");
            m_Server->KickClient(info.ID);
        }
    }

    void NetServerSystem::OnDisconnected(const net::ClientInfo &info)
    {
        auto it = m_Clients.find(info.ID);
        if (it == m_Clients.end())
            return;

        RemoteClient client = std::move(it->second);
        m_Clients.erase(it);

        if (client.handshaked)
        {
            Log("Client left: " + client.name + " (id " + std::to_string(client.clientId) + ")");
            if (m_OnLeft)
                m_OnLeft(client);
        }
    }

    void NetServerSystem::OnData(const net::ClientInfo &info, net::Buffer buffer)
    {
        auto it = m_Clients.find(info.ID);
        if (it == m_Clients.end())
            return;

        RemoteClient &client = it->second;
        net::BufferStreamReader reader(buffer);
        const netproto::MsgType type = netproto::ReadMsgType(reader);

        // Until the handshake completes only Hello is accepted.
        if (!client.handshaked && type != netproto::MsgType::Hello)
            return;

        switch (type)
        {
        case netproto::MsgType::Hello: HandleHello(client, reader); break;
        case netproto::MsgType::Input: HandleInput(client, reader); break;
        case netproto::MsgType::Chat:  HandleChat(client, reader); break;
        case netproto::MsgType::Ping:  HandlePing(client, reader); break;
        default:
            // Unknown or server-only message from a client: ignore.
            break;
        }
    }

    void NetServerSystem::HandleHello(RemoteClient &client, net::StreamReader &reader)
    {
        if (client.handshaked)
            return; // duplicate Hello

        netproto::HelloMsg hello;
        if (!hello.Read(reader))
        {
            Reject(client.connection, "Malformed Hello");
            m_Server->KickClient(client.connection);
            return;
        }

        if (hello.version != netproto::PROTOCOL_VERSION)
        {
            Reject(client.connection, "Protocol version mismatch (server " + std::to_string(netproto::PROTOCOL_VERSION)
                + ", client " + std::to_string(hello.version) + ")");
            m_Server->KickClient(client.connection);
            return;
        }

        client.name = SanitizeName(hello.name, kMaxNameLength);
        if (client.name.empty())
            client.name = "Player";
        client.clientId = m_NextClientId++;
        client.handshaked = true;

        netproto::WelcomeMsg welcome;
        welcome.clientId = client.clientId;
        welcome.serverTick = m_Tick;
        welcome.tickRate = m_Config.tickRate;
        welcome.snapshotRate = m_Config.snapshotRate;
        Send(client.connection, [&](net::StreamWriter &w) { welcome.Write(w); }, true);

        Log("Client joined: " + client.name + " (id " + std::to_string(client.clientId) + ")");
        if (m_OnJoined)
            m_OnJoined(client);
    }

    void NetServerSystem::HandleInput(RemoteClient &client, net::StreamReader &reader)
    {
        netproto::InputMsg msg;
        if (!msg.Read(reader))
            return;

        for (const auto &cmd : msg.cmds)
        {
            // Inputs are sent redundantly; accept each sequence number once and in order.
            if (cmd.seq <= client.lastInputSeq)
                continue;

            client.lastInputSeq = cmd.seq;
            if (client.pendingInputs.size() >= kMaxPendingInputs)
                client.pendingInputs.pop_front();
            client.pendingInputs.push_back(cmd);
        }
    }

    void NetServerSystem::HandleChat(RemoteClient &client, net::StreamReader &reader)
    {
        netproto::ChatMsg msg;
        if (!msg.Read(reader))
            return;

        const auto now = std::chrono::steady_clock::now();
        if (client.lastChatAt != std::chrono::steady_clock::time_point{}
            && std::chrono::duration<double>(now - client.lastChatAt).count() < m_Config.chatMinIntervalSec)
            return; // rate limited
        client.lastChatAt = now;

        msg.clientId = client.clientId;
        msg.text = SanitizeName(msg.text, netproto::ChatMsg::MAX_LENGTH);
        if (msg.text.empty())
            return;

        if (m_OnChat)
            m_OnChat(client, msg);

        Broadcast([&](net::StreamWriter &w) { msg.Write(w); }, true);
    }

    void NetServerSystem::HandlePing(RemoteClient &client, net::StreamReader &reader)
    {
        netproto::PingMsg ping;
        if (!ping.Read(reader))
            return;

        Send(client.connection, [&](net::StreamWriter &w) { ping.Write(w, netproto::MsgType::Pong); }, false);
    }

    void NetServerSystem::Log(const std::string &msg) const
    {
        if (m_Log)
            m_Log("[NetServer] " + msg);
    }

    bool NetServerSystem::GetClientRealTimeStatus(uint32_t clientId, SteamNetConnectionRealTimeStatus_t *status) const
    {
        if (m_Server)
            return m_Server->GetClientRealTimeStatus(clientId, status);
        return false;
    }
}
