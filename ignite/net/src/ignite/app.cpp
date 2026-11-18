// Copyright (c) 2026 Evangelion Manuhutu

#include "app.hpp"
#include <ignite/core/logger.hpp>
#include <ignite/core/vfs/vfs.hpp>
#include <ignite/scene/scene_manager.hpp>
#include <ignite/asset/asset_manager.hpp>
#include <ignite/serializer/scene_serializer.hpp>

#include <filesystem>
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

    static std::filesystem::path FindPackagedProjectInDirectory(const std::filesystem::path &dir)
    {
        std::error_code ec;
        if (dir.empty() || !std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
            return {};

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (ec) break;
            if (entry.is_regular_file(ec) && entry.path().extension() == ".ixproj")
                return entry.path();
        }
        return {};
    }

    ServerAppConfig ParseServerCommandLine(int argc, char **argv)
    {
        ServerAppConfig cfg;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            auto matchOption = [&](const char *flag, std::string &outValue) -> bool
            {
                const std::string prefix = std::string(flag) + "=";
                if (arg.rfind(prefix, 0) == 0)
                {
                    outValue = arg.substr(prefix.size());
                    return true;
                }
                if (arg == flag && i + 1 < argc)
                {
                    outValue = argv[++i];
                    return true;
                }
                return false;
            };

            std::string val;
            if (matchOption("--port", val) || matchOption("-p", val))
            {
                cfg.port = std::stoi(val);
                cfg.hasExplicitPort = true;
            }
            else if (matchOption("--project", val))
            {
                cfg.projectPath = val;
            }
            else if (matchOption("--scene", val))
            {
                cfg.scenePath = val;
            }
            else if (matchOption("--tickrate", val))
            {
                cfg.tickRate = static_cast<uint16_t>(std::stoi(val));
                cfg.hasExplicitTickRate = true;
            }
            else if (matchOption("--snapshotrate", val))
            {
                cfg.snapshotRate = static_cast<uint16_t>(std::stoi(val));
                cfg.hasExplicitSnapshotRate = true;
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

    std::string ServerApp::ResolveDefaultPlayerTemplate() const
    {
        if (m_Scene && m_Scene->registry)
        {
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                const auto &idComp = view.get<NetworkIdentityComponent>(e);
                if (!idComp.isLocalPlayer)
                    continue;

                if (!idComp.prefab.empty())
                    return idComp.prefab;

                Entity entity{ e, m_Scene.get() };
                if (!entity.GetName().empty())
                    return entity.GetName();
            }
        }
        return "Player";
    }

    bool ServerApp::Initialize()
    {
        std::error_code ec;
        const std::filesystem::path exeDir = vfs::GetExecutableDirectory();
        if (!exeDir.empty() && std::filesystem::exists(exeDir / "resources", ec))
        {
            std::filesystem::current_path(exeDir, ec);
        }

        if (!m_AssetManager)
        {
            m_AssetManager = std::make_unique<AssetManager>();
            m_AssetManager->Init();
        }

        if (m_Config.projectPath.empty())
        {
            std::filesystem::path discovered = FindPackagedProjectInDirectory(std::filesystem::current_path(ec));
            if (discovered.empty())
                discovered = FindPackagedProjectInDirectory(exeDir);
            if (!discovered.empty())
                m_Config.projectPath = discovered.string();
        }

        if (!m_Config.projectPath.empty())
        {
            std::filesystem::path resolvedProj = std::filesystem::absolute(m_Config.projectPath, ec);
            m_Project = Project::Open(ec ? std::filesystem::path(m_Config.projectPath) : resolvedProj);
            if (m_Project)
            {
                const auto &ss = m_Project->GetServerSettings();
                if (!m_Config.hasExplicitPort)
                    m_Config.port = static_cast<int>(ss.port);
                if (!m_Config.hasExplicitTickRate)
                    m_Config.tickRate = ss.tickRate;
                if (!m_Config.hasExplicitSnapshotRate)
                    m_Config.snapshotRate = ss.snapshotRate;
                if (m_Config.scenePath.empty() && !ss.defaultScene.empty())
                    m_Config.scenePath = ss.defaultScene;

                AssetHandle defSceneHandle = m_Project->GetInfo().defaultSceneHandle;
                if (defSceneHandle != AssetHandle(0))
                {
                    if (AssetManager *am = AssetManager::GetInstance())
                    {
                        if (Ref<Scene> activeScene = am->GetAssetImmediate<Scene>(defSceneHandle))
                        {
                            m_Scene = SceneManager::Copy(activeScene);
                            m_Project->SetActiveScene(m_Scene);
                        }
                    }
                }
            }
        }

        if (!m_Config.scenePath.empty())
        {
            std::filesystem::path resolvedScenePath = m_Config.scenePath;
            if (!std::filesystem::exists(resolvedScenePath) && m_Project)
            {
                const std::filesystem::path candidate = m_Project->GetAssetDirectory() / resolvedScenePath;
                if (std::filesystem::exists(candidate))
                    resolvedScenePath = candidate;
                else
                {
                    const std::filesystem::path projCandidate = m_Project->GetDirectory() / resolvedScenePath;
                    if (std::filesystem::exists(projCandidate))
                        resolvedScenePath = projCandidate;
                }
            }

            if (std::filesystem::exists(resolvedScenePath))
            {
                if (Ref<Scene> loadedScene = SceneSerializer::Deserialize(resolvedScenePath, m_Project.get()))
                {
                    m_Scene = loadedScene;
                    if (m_Project)
                        m_Project->SetActiveScene(m_Scene);
                }
            }
            else
            {
                LOG_WARN("[ServerApp] Scene file '{}' not found", m_Config.scenePath);
            }
        }

        if (!m_Scene)
        {
            m_Scene = Scene::Create(m_Project.get());
        }

        if (m_Project)
        {
            m_Project->InitScriptEngine();
        }

        m_NetworkManager = std::make_unique<NetworkManager>();
        m_NetworkManager->Init();
        m_NetworkManager->SetActiveScene(m_Scene.get());

        // Start runtime gameplay scripts (e.g. server_manager.lua / C# scripts) before accepting joins
        m_Scene->OnStart(ESceneState::Play);

        // Fallback spawner when scene scripts (on_network_client_joined / OnNetworkClientJoined) do not spawn an owned avatar
        m_NetworkManager->SetClientJoinedCallback([this](uint32_t clientId, const std::string &clientName) {
            if (!m_Scene || !m_NetworkManager || !m_NetworkManager->GetServerReplication())
                return;

            bool alreadyHasOwnedEntity = false;
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                if (view.get<NetworkIdentityComponent>(e).ownerClient == clientId)
                {
                    alreadyHasOwnedEntity = true;
                    break;
                }
            }

            if (!alreadyHasOwnedEntity)
            {
                const std::string templateName = ResolveDefaultPlayerTemplate();
                LOG_INFO("[ServerApp] Auto-spawning template '{}' for joined client {} ({})", templateName, clientName, clientId);
                Entity player = m_NetworkManager->GetServerReplication()->SpawnEntity(
                    templateName, glm::vec3(0.0f, 1.35f, -3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), clientId);
                if (player.IsValid() && !player.HasComponent<CharacterControllerComponent>())
                {
                    auto &cc = player.AddComponent<CharacterControllerComponent>();
                    cc.dirty = true;
                }
            }
        });

        NetServerSystem::Config netCfg;
        netCfg.port = m_Config.port;
        netCfg.tickRate = m_Config.tickRate;
        netCfg.snapshotRate = m_Config.snapshotRate;
        if (m_Project)
        {
            const auto &ss = m_Project->GetServerSettings();
            netCfg.maxClients = ss.maxClients;
            netCfg.handshakeTimeoutSec = ss.handshakeTimeoutSec;
            netCfg.chatMinIntervalSec = ss.chatMinIntervalSec;
        }

        if (!m_NetworkManager->StartServer(netCfg))
        {
            LOG_ERROR("Failed to start net server on port {}", m_Config.port);
            return false;
        }

        // StartServer creates NetServerReplication which marks isLocalPlayer templates replicated by default.
        // Headless dedicated server has no local player, so keep scene player templates un-replicated to avoid phantom Player_0 clones.
        if (m_Scene && m_Scene->registry)
        {
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                auto &netComp = view.get<NetworkIdentityComponent>(e);
                if (netComp.isLocalPlayer && netComp.ownerClient == 0)
                {
                    netComp.replicate = false;
                    if (netComp.netId != 0)
                    {
                        m_Scene->UnregisterNetEntity(netComp.netId);
                        netComp.netId = 0;
                    }
                }
            }
        }

        return true;
    }

    void ServerApp::Stop()
    {
        m_Running = false;
        if (m_NetworkManager)
        {
            m_NetworkManager->StopServer();
        }
        if (m_Scene && m_Scene->IsRunning())
        {
            m_Scene->OnStop();
        }
        m_Scene.reset();
        m_Project.reset();
        m_NetworkManager.reset();
        if (m_AssetManager)
        {
            m_AssetManager->Shutdown();
            m_AssetManager.reset();
        }
    }

    void ServerApp::Run()
    {
        m_Running = true;

        const float tickDt = 1.0f / static_cast<float>(std::max<uint16_t>(1, m_Config.tickRate));
        const auto tickDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<double>(tickDt));

        auto nextTick = std::chrono::steady_clock::now();

        LOG_INFO("Dedicated server running at {} Hz (port {})", m_Config.tickRate, m_Config.port);

        while (m_Running)
        {
            if (m_AssetManager)
            {
                m_AssetManager->OnUpdate(tickDt);
            }

            if (m_NetworkManager)
            {
                m_NetworkManager->Update(tickDt);
            }

            if (m_Scene && m_Scene->IsRunning())
            {
                m_Scene->OnFixedUpdateRuntimeSimulate();
                m_Scene->OnUpdateRuntimeSimulate(tickDt);
            }

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
