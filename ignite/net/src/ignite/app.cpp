// Copyright (c) 2026 Evangelion Manuhutu

#include "app.hpp"
#include <ignite/core/logger.hpp>
#include <ignite/scene/scene_manager.hpp>
#include <ignite/asset/asset_manager.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

namespace ignite
{
    static ServerApp *s_AppInstance = nullptr;

    static void SignalHandler(int signum)
    {
        (void)signum;
        if (s_AppInstance)
        {
            s_AppInstance->Stop();
        }
    }

    ServerAppConfig ParseServerCommandLine(int argc, char **argv)
    {
        ServerAppConfig cfg;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if ((arg == "--port" || arg == "-p") && i + 1 < argc)
            {
                cfg.port = std::stoi(argv[++i]);
            }
            else if ((arg == "--project") && i + 1 < argc)
            {
                cfg.projectPath = argv[++i];
            }
            else if ((arg == "--scene") && i + 1 < argc)
            {
                cfg.scenePath = argv[++i];
            }
            else if ((arg == "--tickrate") && i + 1 < argc)
            {
                cfg.tickRate = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
            else if ((arg == "--snapshotrate") && i + 1 < argc)
            {
                cfg.snapshotRate = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
        }
        return cfg;
    }

    ServerApp::ServerApp(const ServerAppConfig &config)
        : m_Config(config)
    {
        s_AppInstance = this;
    }

    ServerApp::~ServerApp()
    {
        Stop();
        if (s_AppInstance == this)
            s_AppInstance = nullptr;
    }

    bool ServerApp::Initialize()
    {
        NetServerSystem::Config netCfg;
        netCfg.port = m_Config.port;
        netCfg.tickRate = m_Config.tickRate;
        netCfg.snapshotRate = m_Config.snapshotRate;

        m_ServerSystem = std::make_unique<NetServerSystem>(netCfg);
        m_ServerSystem->SetLogCallback([](const std::string &msg) {
            LOG_INFO("{}", msg);
        });

        if (!m_Config.projectPath.empty())
        {
            m_Project = Project::Open(m_Config.projectPath);
            if (m_Project)
            {
                AssetHandle defSceneHandle = m_Project->GetInfo().defaultSceneHandle;
                if (defSceneHandle != AssetHandle(0))
                {
                    if (Ref<Scene> activeScene = AssetManager::GetInstance()->GetAssetImmediate<Scene>(defSceneHandle))
                    {
                        m_Scene = SceneManager::Copy(activeScene);
                        m_Project->SetActiveScene(m_Scene);
                    }
                }
            }
        }

        if (!m_Scene)
        {
            m_Scene = Scene::Create(m_Project.get());
        }

        m_Replication = std::make_unique<NetServerReplication>(m_Scene.get(), m_ServerSystem.get());

        m_ServerSystem->SetClientJoinedCallback([this](NetServerSystem::RemoteClient &client) {
            LOG_INFO("Spawn world for joined client {}", client.name);
            m_Replication->SyncWorldToClient(client.clientId);

            // Spawn player entity for this client
            Entity player = m_Replication->SpawnEntity("Player", glm::vec3(0.0f, 1.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), client.clientId);
            if (player.IsValid())
            {
                if (!player.HasComponent<CharacterControllerComponent>())
                {
                    auto &cc = player.AddComponent<CharacterControllerComponent>();
                    cc.dirty = true;
                }
            }
        });

        m_ServerSystem->SetClientLeftCallback([this](NetServerSystem::RemoteClient &client) {
            LOG_INFO("Despawn player for client {}", client.name);
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                const auto &netComp = view.get<NetworkIdentityComponent>(e);
                if (netComp.ownerClient == client.clientId)
                {
                    m_Replication->DespawnEntity(netComp.netId);
                    break;
                }
            }
        });

        if (!m_ServerSystem->Start())
        {
            LOG_ERROR("Failed to start net server on port {}", m_Config.port);
            return false;
        }

        m_Scene->OnStart(ESceneState::Play);
        return true;
    }

    void ServerApp::Stop()
    {
        m_Running = false;
        if (m_ServerSystem)
        {
            m_ServerSystem->Stop();
        }
        if (m_Scene)
        {
            m_Scene->OnStop();
        }
    }

    void ServerApp::Run()
    {
        m_Running = true;

        const float tickDt = 1.0f / static_cast<float>(m_Config.tickRate);
        const auto tickDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(tickDt));

        const uint32_t ticksPerSnapshot = std::max<uint32_t>(1, m_Config.tickRate / m_Config.snapshotRate);

        auto nextTick = std::chrono::steady_clock::now();

        LOG_INFO("Dedicated server running at {} Hz (port {})", m_Config.tickRate, m_Config.port);

        while (m_Running)
        {
            // 1. Pump network events
            m_ServerSystem->Tick();

            // 2. Apply queued client inputs to character controllers
            m_ServerSystem->ForEachClient([this](NetServerSystem::RemoteClient &client) {
                auto inputs = m_ServerSystem->DrainInputs(client.clientId);
                if (inputs.empty())
                    return;

                // Find this client's player entity
                auto view = m_Scene->registry->view<NetworkIdentityComponent, CharacterControllerComponent>();
                for (entt::entity e : view)
                {
                    const auto &[netComp, cc] = view.get<NetworkIdentityComponent, CharacterControllerComponent>(e);
                    if (netComp.ownerClient == client.clientId)
                    {
                        // Apply newest input movement
                        const auto &latest = inputs.back();
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
                        else if (latest.buttons & netproto::InputButton_Jump)
                        {
                            cc.linearVelocity.y = 5.0f;
                        }
                        break;
                    }
                }
            });

            // 3. Step Scene physics & simulation
            m_Scene->OnFixedUpdateRuntimeSimulate();
            m_Scene->OnUpdateRuntimeSimulate(tickDt);

            // 4. Periodically broadcast world snapshots
            if (m_ServerSystem->GetTick() % ticksPerSnapshot == 0)
            {
                m_Replication->BroadcastSnapshots();
            }

            // 5. High-resolution sleep to next tick
            nextTick += tickDuration;
            const auto now = std::chrono::steady_clock::now();
            if (nextTick > now)
            {
                std::this_thread::sleep_for(nextTick - now);
            }
            else
            {
                nextTick = now;
            }
        }

        LOG_INFO("Dedicated server shutting down...");
    }
}

int main(int argc, char **argv)
{
    ignite::Logger::Init();
    std::signal(SIGINT, ignite::SignalHandler);
    std::signal(SIGTERM, ignite::SignalHandler);

    ignite::ServerAppConfig config = ignite::ParseServerCommandLine(argc, argv);
    ignite::ServerApp app(config);

    if (app.Initialize())
    {
        app.Run();
    }

    ignite::Logger::Shutdown();
    return 0;
}
