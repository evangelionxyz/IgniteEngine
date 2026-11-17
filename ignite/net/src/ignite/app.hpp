// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_APP_HPP
#define IGN_NET_APP_HPP

#include <ignite/net/net_server_system.hpp>
#include <ignite/net/net_replication.hpp>
#include <ignite/net/network_manager.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/project/project.hpp>

#include <memory>
#include <string>
#include <atomic>

namespace ignite
{
    struct ServerAppConfig
    {
        int port = 8192;
        uint16_t tickRate = 60;
        uint16_t snapshotRate = 20;
        bool hasExplicitPort = false;
        bool hasExplicitTickRate = false;
        bool hasExplicitSnapshotRate = false;
        std::string projectPath;
        std::string scenePath;
    };

    class ServerApp
    {
    public:
        explicit ServerApp(const ServerAppConfig &config);
        ~ServerApp();

        bool Initialize();
        void Run();
        void Stop();

        Scene *GetScene() { return m_Scene.get(); }
        NetServerSystem *GetServerSystem() { return m_NetworkManager ? m_NetworkManager->GetServerSystem() : nullptr; }
        NetServerReplication *GetReplication() { return m_NetworkManager ? m_NetworkManager->GetServerReplication() : nullptr; }
        NetworkManager *GetNetworkManager() { return m_NetworkManager.get(); }

    private:
        std::string ResolveDefaultPlayerTemplate() const;

        ServerAppConfig m_Config;
        std::atomic<bool> m_Running{ false };

        Ref<Project> m_Project;
        Ref<Scene> m_Scene;
        std::unique_ptr<NetworkManager> m_NetworkManager;
    };

    ServerAppConfig ParseServerCommandLine(int argc, char **argv);
}

#endif
