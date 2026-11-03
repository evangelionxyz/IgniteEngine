// Copyright (c) 2026 Evangelion Manuhutu

#include "network_manager.hpp"
#include <ignite/core/logger.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/scene/scene_manager.hpp>
#include <ignite/scene/entity.hpp>
#include <ignite/scene/component.hpp>
#include <ignite/project/project.hpp>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <imgui.h>

namespace ignite
{
    NetworkManager *NetworkManager::s_Instance = nullptr;

    NetworkManager::NetworkManager()
    {
        s_Instance = this;
    }

    NetworkManager::~NetworkManager()
    {
        Shutdown();
        if (s_Instance == this)
        {
            s_Instance = nullptr;
        }
    }

    NetworkManager *NetworkManager::GetInstance()
    {
        return s_Instance;
    }

    void NetworkManager::Init()
    {
        LOG_INFO("[NetworkManager] Initialized");
    }

    void NetworkManager::Shutdown()
    {
        Disconnect();
        StopServer();
        SetSimulationSettings(0, 0.0f);
    }

    void NetworkManager::SetActiveScene(Scene *scene)
    {
        m_ActiveScene = scene;

        if (m_ClientSystem && m_ClientSystem->IsConnected())
        {
            if (m_ActiveScene)
                m_ActiveScene->SetNetMode(ENetMode::Client);
            m_ClientReplication = std::make_unique<NetClientReplication>(m_ActiveScene, m_ClientSystem.get());
        }
        else if (m_ServerSystem && m_ServerSystem->IsRunning())
        {
            if (m_ActiveScene)
                m_ActiveScene->SetNetMode(ENetMode::DedicatedServer);
            m_ServerReplication = std::make_unique<NetServerReplication>(m_ActiveScene, m_ServerSystem.get());
        }
    }

    bool NetworkManager::Connect(const std::string &address, uint16_t port, const std::string &playerName)
    {
        StopServer();
        Disconnect();

        if (!m_ActiveScene)
        {
            if (auto project = Project::GetActive())
            {
                if (auto activeScene = project->GetActiveSceneWeak().lock())
                {
                    m_ActiveScene = activeScene.get();
                }
            }
        }

        if (m_ActiveScene)
        {
            m_ActiveScene->SetNetMode(ENetMode::Client);
        }

        m_ClientSystem = std::make_unique<NetClientSystem>();
        m_ClientSystem->SetLogCallback([](const std::string &msg) {
            LOG_INFO("{}", msg);
        });

        m_ClientReplication = std::make_unique<NetClientReplication>(m_ActiveScene, m_ClientSystem.get());

        m_ClientSystem->onChat = [this](const netproto::ChatMsg &msg) {
            NetChatMessage chatMsg;
            chatMsg.senderId = msg.clientId;
            chatMsg.senderName = (msg.clientId == 0) ? "Server" : ("Client_" + std::to_string(msg.clientId));
            chatMsg.text = msg.text;
            m_ChatHistory.push_back(chatMsg);
            if (m_ChatHistory.size() > 100)
                m_ChatHistory.erase(m_ChatHistory.begin());

            if (m_ChatCallback)
                m_ChatCallback(chatMsg);
        };

        const std::string fullAddress = address + ":" + std::to_string(port);
        m_ClientSystem->Connect(fullAddress, playerName);
        return true;
    }

    void NetworkManager::Disconnect()
    {
        m_InputHistory.clear();

        if (m_ClientSystem)
        {
            m_ClientSystem->Disconnect();
            m_ClientSystem.reset();
        }

        m_ClientReplication.reset();

        if (m_ActiveScene && m_ActiveScene->GetNetMode() == ENetMode::Client)
        {
            m_ActiveScene->SetNetMode(ENetMode::Standalone);
        }
    }

    bool NetworkManager::IsConnected() const
    {
        return m_ClientSystem && m_ClientSystem->IsConnected();
    }

    bool NetworkManager::IsConnecting() const
    {
        return m_ClientSystem && (m_ClientSystem->GetState() == NetClientSystem::State::Connecting ||
                                  m_ClientSystem->GetState() == NetClientSystem::State::Handshaking);
    }

    uint32_t NetworkManager::GetClientId() const
    {
        return m_ClientSystem ? m_ClientSystem->GetClientId() : 0;
    }

    bool NetworkManager::StartServer()
    {
        NetServerSystem::Config cfg;
        if (auto project = Project::GetActive())
        {
            const auto &s = project->GetInfo().serverSettings;
            cfg.port = s.port;
            cfg.tickRate = s.tickRate;
            cfg.snapshotRate = s.snapshotRate;
            cfg.maxClients = s.maxClients;
            cfg.handshakeTimeoutSec = s.handshakeTimeoutSec;
            cfg.chatMinIntervalSec = s.chatMinIntervalSec;
        }
        return StartServer(cfg);
    }

    bool NetworkManager::StartServer(uint16_t port, uint16_t tickRate, uint16_t snapshotRate)
    {
        NetServerSystem::Config cfg;
        cfg.port = port;
        cfg.tickRate = tickRate;
        cfg.snapshotRate = snapshotRate;
        return StartServer(cfg);
    }

    bool NetworkManager::StartServer(const NetServerSystem::Config &config)
    {
        Disconnect();
        StopServer();

        if (!m_ActiveScene)
        {
            if (auto project = Project::GetActive())
            {
                if (auto activeScene = project->GetActiveSceneWeak().lock())
                {
                    m_ActiveScene = activeScene.get();
                }
            }
        }

        if (m_ActiveScene)
        {
            m_ActiveScene->SetNetMode(ENetMode::DedicatedServer);
        }

        m_ServerSystem = std::make_unique<NetServerSystem>(config);
        m_ServerSystem->SetLogCallback([](const std::string &msg) {
            LOG_INFO("{}", msg);
        });

        m_ServerReplication = std::make_unique<NetServerReplication>(m_ActiveScene, m_ServerSystem.get());

        m_ServerSystem->SetClientJoinedCallback([this](NetServerSystem::RemoteClient &client) {
            LOG_INFO("[NetworkManager] Client joined: {} (ID: {})", client.name, client.clientId);
            if (m_ServerReplication)
            {
                m_ServerReplication->SyncWorldToClient(client.clientId);
                // Automatically spawn a player entity for this client
                m_ServerReplication->SpawnEntity("Player_" + client.name, glm::vec3(0.0f, 1.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), client.clientId);
            }
            if (m_ClientJoinedCallback)
            {
                m_ClientJoinedCallback(client.clientId, client.name);
            }
        });

        m_ServerSystem->SetClientLeftCallback([this](NetServerSystem::RemoteClient &client) {
            LOG_INFO("[NetworkManager] Client left: {} (ID: {})", client.name, client.clientId);
            if (m_ActiveScene && m_ServerReplication)
            {
                // Despawn owned entities
                auto view = m_ActiveScene->registry->view<NetworkIdentityComponent>();
                std::vector<uint64_t> toDespawn;
                for (entt::entity e : view)
                {
                    const auto &comp = view.get<NetworkIdentityComponent>(e);
                    if (comp.ownerClient == client.clientId)
                        toDespawn.push_back(comp.netId);
                }
                for (uint64_t netId : toDespawn)
                {
                    m_ServerReplication->DespawnEntity(netId);
                }
            }
            if (m_ClientLeftCallback)
            {
                m_ClientLeftCallback(client.clientId, client.name);
            }
        });

        m_ServerSystem->SetChatCallback([this](NetServerSystem::RemoteClient &client, const netproto::ChatMsg &msg) {
            NetChatMessage chatMsg;
            chatMsg.senderId = client.clientId;
            chatMsg.senderName = client.name;
            chatMsg.text = msg.text;
            m_ChatHistory.push_back(chatMsg);
            if (m_ChatHistory.size() > 100)
                m_ChatHistory.erase(m_ChatHistory.begin());
        });

        if (!m_ServerSystem->Start())
        {
            LOG_ERROR("[NetworkManager] Failed to start server on port {}", config.port);
            m_ServerSystem.reset();
            m_ServerReplication.reset();
            return false;
        }

        m_ServerTickAccumulator = 0.0f;
        m_ServerSnapshotAccumulator = 0.0f;
        return true;
    }

    void NetworkManager::KickClient(uint32_t clientId, const std::string &reason)
    {
        if (m_ServerSystem)
        {
            m_ServerSystem->Kick(clientId, reason);
        }
    }

    size_t NetworkManager::GetClientCount() const
    {
        return m_ServerSystem ? m_ServerSystem->GetClientCount() : 0;
    }

    std::vector<uint32_t> NetworkManager::GetConnectedClientIds() const
    {
        return m_ServerSystem ? m_ServerSystem->GetConnectedClientIds() : std::vector<uint32_t>{};
    }

    std::vector<std::string> NetworkManager::GetConnectedClientNames() const
    {
        return m_ServerSystem ? m_ServerSystem->GetConnectedClientNames() : std::vector<std::string>{};
    }

    uint32_t NetworkManager::GetServerTick() const
    {
        return m_ServerSystem ? m_ServerSystem->GetTick() : 0;
    }

    uint32_t NetworkManager::GetClientServerTick() const
    {
        return m_ClientSystem ? m_ClientSystem->GetServerTick() : 0;
    }

    void NetworkManager::StopServer()
    {
        if (m_ServerSystem)
        {
            m_ServerSystem->Stop();
            m_ServerSystem.reset();
        }

        m_ServerReplication.reset();

        if (m_ActiveScene && m_ActiveScene->GetNetMode() == ENetMode::DedicatedServer)
        {
            m_ActiveScene->SetNetMode(ENetMode::Standalone);
        }
    }

    bool NetworkManager::IsServerRunning() const
    {
        return m_ServerSystem && m_ServerSystem->IsRunning();
    }

    void NetworkManager::Update(float deltaTime)
    {
        // 1. Client processing
        if (m_ClientSystem)
        {
            m_ClientSystem->Tick();
            if (m_ClientReplication)
            {
                m_ClientReplication->Update(deltaTime);
            }
        }

        // 2. Server processing
        if (m_ServerSystem && m_ServerSystem->IsRunning())
        {
            const float tickInterval = 1.0f / static_cast<float>(m_ServerSystem->GetConfig().tickRate);
            const float snapInterval = 1.0f / static_cast<float>(m_ServerSystem->GetConfig().snapshotRate);

            m_ServerTickAccumulator += deltaTime;
            m_ServerSnapshotAccumulator += deltaTime;

            while (m_ServerTickAccumulator >= tickInterval)
            {
                m_ServerTickAccumulator -= tickInterval;
                m_ServerSystem->Tick();

                // Consume player input commands
                m_ServerSystem->ForEachClient([this](NetServerSystem::RemoteClient &client) {
                    auto inputs = m_ServerSystem->DrainInputs(client.clientId);
                    if (inputs.empty() || !m_ActiveScene)
                        return;

                    const auto &latest = inputs.back();
                    auto view = m_ActiveScene->registry->view<NetworkIdentityComponent>();
                    for (entt::entity e : view)
                    {
                        const auto &netComp = view.get<NetworkIdentityComponent>(e);
                        if (netComp.ownerClient == client.clientId)
                        {
                            Entity entity{ e, m_ActiveScene };
                            if (entity.HasComponent<CharacterControllerComponent>())
                            {
                                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                                const glm::vec3 moveDir(latest.move.x, 0.0f, latest.move.y);
                                constexpr float speed = 5.0f;
                                cc.linearVelocity.x = moveDir.x * speed;
                                cc.linearVelocity.z = moveDir.z * speed;
                                if (auto charActor = cc.character.lock())
                                {
                                    if ((latest.buttons & netproto::InputButton_Jump) && charActor->IsOnGround())
                                    {
                                        cc.linearVelocity.y = 5.0f;
                                    }
                                }
                            }
                            else
                            {
                                auto &tr = entity.GetTransform();
                                tr.local.translation += glm::vec3(latest.move.x, 0.0f, latest.move.y) * 5.0f * (1.0f / 60.0f);
                            }
                            break;
                        }
                    }
                });

                if (m_ActiveScene && m_ActiveScene->IsRunning())
                {
                    m_ActiveScene->OnFixedUpdateRuntimeSimulate();
                }
            }

            if (m_ServerSnapshotAccumulator >= snapInterval)
            {
                m_ServerSnapshotAccumulator -= snapInterval;
                if (m_ServerReplication)
                {
                    m_ServerReplication->BroadcastSnapshots();
                    m_ServerReplication->BroadcastDirtyComponents();
                }
            }
        }
    }

    void NetworkManager::SendInput(const glm::vec2 &move, uint16_t buttons, float yaw)
    {
        netproto::InputCmd cmd;
        cmd.seq = m_NextInputSeq++;
        cmd.move = move;
        cmd.buttons = buttons;
        cmd.yaw = yaw;

        SendInput(cmd);
    }

    void NetworkManager::SendInput(const netproto::InputCmd &cmd)
    {
        if (!m_ClientSystem || !m_ClientSystem->IsConnected())
            return;

        // Predict local player movement
        if (m_ClientReplication)
        {
            Entity localPlayer = m_ClientReplication->GetLocalPlayerEntity();
            if (localPlayer.IsValid())
            {
                auto &tr = localPlayer.GetTransform();
                glm::vec3 predictedPos = tr.local.translation;
                glm::quat predictedRot = tr.local.rotation;

                predictedPos += glm::vec3(cmd.move.x, 0.0f, cmd.move.y) * 5.0f * (1.0f / 60.0f);

                m_ClientReplication->GetPrediction().Record(
                    cmd.seq, m_ClientSystem->GetServerTick(),
                    predictedPos, predictedRot, cmd);

                tr.local.translation = predictedPos;
                tr.local.rotation = predictedRot;
                tr.world.translation = predictedPos;
                tr.world.rotation = predictedRot;
            }
        }

        m_InputHistory.push_back(cmd);
        if (m_InputHistory.size() > netproto::MAX_INPUTS_PER_PACKET)
            m_InputHistory.erase(m_InputHistory.begin());

        netproto::InputMsg msg;
        msg.cmds = m_InputHistory;

        // Send unreliable packet over wire with full InputMsg framing
        m_ClientSystem->Send([&](net::StreamWriter &w) {
            msg.Write(w);
        }, false);
    }

    void NetworkManager::SendChat(const std::string &text)
    {
        if (m_ClientSystem && m_ClientSystem->IsConnected())
        {
            m_ClientSystem->SendChat(text);
        }
        else if (m_ServerSystem && m_ServerSystem->IsRunning())
        {
            netproto::ChatMsg msg;
            msg.clientId = 0; // Server
            msg.text = text;

            NetChatMessage chatMsg;
            chatMsg.senderId = 0;
            chatMsg.senderName = "Server";
            chatMsg.text = text;
            m_ChatHistory.push_back(chatMsg);

            m_ServerSystem->Broadcast([&](net::StreamWriter &w) {
                msg.Write(w);
            }, true);
        }
    }

    void NetworkManager::SetSimulationSettings(int lagMs, float lossPercent)
    {
        m_SimLagMs = std::max(0, lagMs);
        m_SimPacketLoss = std::clamp(lossPercent, 0.0f, 100.0f);

        if (SteamNetworkingUtils())
        {
            SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, m_SimLagMs);
            SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Recv, m_SimLagMs);
            SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, m_SimPacketLoss);
            SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Recv, m_SimPacketLoss);
        }
    }

    NetStats NetworkManager::GetStats() const
    {
        NetStats stats;
        if (m_ClientSystem && m_ClientSystem->IsConnected())
        {
            stats.rttMs = static_cast<int>(m_ClientSystem->GetRttMs());
            SteamNetConnectionRealTimeStatus_t rt{};
            if (m_ClientSystem->GetRealTimeStatus(&rt))
            {
                if (rt.m_nPing > 0)
                    stats.rttMs = rt.m_nPing;
                stats.packetLossPercent = (1.0f - rt.m_flConnectionQualityLocal) * 100.0f;
                stats.inKBytesPerSec = rt.m_flInBytesPerSec / 1024.0f;
                stats.outKBytesPerSec = rt.m_flOutBytesPerSec / 1024.0f;
            }
            stats.clientCount = 1;
        }
        else if (m_ServerSystem && m_ServerSystem->IsRunning())
        {
            stats.clientCount = static_cast<uint32_t>(m_ServerSystem->GetClientCount());
        }
        return stats;
    }

    void NetworkManager::OnImGuiRender(bool *pOpen)
    {
        if (pOpen && !*pOpen)
            return;

        if (ImGui::Begin("Network Diagnostics", pOpen))
        {
            // Status Banner
            if (IsServerRunning())
            {
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Mode: Dedicated / Listen Server");
                ImGui::Text("Clients: %zu", m_ServerSystem ? m_ServerSystem->GetClientCount() : 0);
                ImGui::Text("Tick: %u", m_ServerSystem ? m_ServerSystem->GetTick() : 0);
            }
            else if (IsConnected())
            {
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Mode: Client (Connected)");
                ImGui::Text("Client ID: %u | Server Tick: %u", m_ClientSystem->GetClientId(), m_ClientSystem->GetServerTick());
                NetStats stats = GetStats();
                ImGui::Text("RTT: %d ms | Loss: %.1f %%", stats.rttMs, stats.packetLossPercent);
                ImGui::Text("Bandwidth: In: %.1f KB/s | Out: %.1f KB/s", stats.inKBytesPerSec, stats.outKBytesPerSec);
            }
            else if (IsConnecting())
            {
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Mode: Connecting...");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Mode: Standalone / Offline");
            }

            ImGui::Separator();

            // Connection Controls
            if (!IsConnected() && !IsServerRunning())
            {
                ImGui::InputText("Address", m_UiAddressBuf, sizeof(m_UiAddressBuf));
                ImGui::InputInt("Port", &m_UiPort);
                ImGui::InputText("Player Name", m_UiPlayerName, sizeof(m_UiPlayerName));

                if (ImGui::Button("Connect as Client"))
                {
                    Connect(m_UiAddressBuf, static_cast<uint16_t>(m_UiPort), m_UiPlayerName);
                }
                ImGui::SameLine();
                if (ImGui::Button("Host Server"))
                {
                    StartServer(static_cast<uint16_t>(m_UiPort));
                }
            }
            else
            {
                if (IsConnected())
                {
                    if (ImGui::Button("Disconnect"))
                    {
                        Disconnect();
                    }
                }
                if (IsServerRunning())
                {
                    if (ImGui::Button("Stop Server"))
                    {
                        StopServer();
                    }
                }
            }

            ImGui::Separator();

            // Network Simulator (Lag & Loss)
            if (ImGui::CollapsingHeader("Lag & Packet Loss Simulator", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool changed = false;
                changed |= ImGui::SliderInt("Simulated Lag (ms)", &m_SimLagMs, 0, 500);
                changed |= ImGui::SliderFloat("Simulated Packet Loss (%)", &m_SimPacketLoss, 0.0f, 50.0f, "%.1f %%");
                if (changed)
                {
                    SetSimulationSettings(m_SimLagMs, m_SimPacketLoss);
                }
                if (ImGui::Button("Reset Latency & Loss"))
                {
                    m_SimLagMs = 0;
                    m_SimPacketLoss = 0.0f;
                    SetSimulationSettings(0, 0.0f);
                }
            }

            // Replicated Entities
            if (m_ActiveScene && ImGui::CollapsingHeader("Replicated Entities"))
            {
                auto view = m_ActiveScene->registry->view<NetworkIdentityComponent, TransformComponent>();
                size_t repCount = 0;
                for (auto e : view) { (void)e; ++repCount; }
                ImGui::Text("Total Replicated Entities: %zu", repCount);
                if (ImGui::BeginTable("ReplicatedEntitiesTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Net ID");
                    ImGui::TableSetupColumn("Owner");
                    ImGui::TableSetupColumn("Is Local");
                    ImGui::TableSetupColumn("Position");
                    ImGui::TableHeadersRow();

                    for (entt::entity e : view)
                    {
                        const auto &[netComp, tr] = view.get<NetworkIdentityComponent, TransformComponent>(e);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%lu", netComp.netId);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%u", netComp.ownerClient);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", netComp.isLocalPlayer ? "Yes" : "No");
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("(%.2f, %.2f, %.2f)", tr.world.translation.x, tr.world.translation.y, tr.world.translation.z);
                    }
                    ImGui::EndTable();
                }
            }

            // Chat
            if (ImGui::CollapsingHeader("Network Chat", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::BeginChild("ChatMessages", ImVec2(0, 100), true);
                for (const auto &chat : m_ChatHistory)
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s]:", chat.senderName.c_str());
                    ImGui::SameLine();
                    ImGui::Text("%s", chat.text.c_str());
                }
                ImGui::EndChild();

                if (ImGui::InputText("Message", m_UiChatInputBuf, sizeof(m_UiChatInputBuf), ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    if (m_UiChatInputBuf[0] != '\0')
                    {
                        SendChat(m_UiChatInputBuf);
                        m_UiChatInputBuf[0] = '\0';
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Send"))
                {
                    if (m_UiChatInputBuf[0] != '\0')
                    {
                        SendChat(m_UiChatInputBuf);
                        m_UiChatInputBuf[0] = '\0';
                    }
                }
            }
        }
        ImGui::End();
    }
}

