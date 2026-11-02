// Copyright (c) 2026 Evangelion Manuhutu

#include "net_replication.hpp"
#include <ignite/scene/scene_manager.hpp>

namespace ignite
{
    // =========================================================================
    // Server-Side Replication Implementation
    // =========================================================================
    NetServerReplication::NetServerReplication(Scene *scene, NetServerSystem *serverSystem)
        : m_Scene(scene), m_Server(serverSystem)
    {
        if (m_Scene)
        {
            m_Scene->SetNetMode(ENetMode::DedicatedServer);
        }
    }

    NetServerReplication::~NetServerReplication()
    {
    }

    Entity NetServerReplication::SpawnEntity(const std::string &prefab, const glm::vec3 &position, const glm::quat &rotation, uint32_t ownerClient)
    {
        if (!m_Scene || !m_Server)
            return {};

        const uint64_t netId = GenerateNetId();
        const std::string name = prefab.empty() ? ("NetEntity_" + std::to_string(netId)) : prefab;

        Entity entity = SceneManager::CreateEntity(m_Scene, name, EntityType_Node);
        auto &tr = entity.GetTransform();
        tr.local.translation = position;
        tr.local.rotation = rotation;
        tr.world.translation = position;
        tr.world.rotation = rotation;

        entity.AddComponent<NetworkIdentityComponent>(netId, ownerClient, false, prefab);

        // Broadcast spawn to all connected clients
        netproto::SpawnMsg msg;
        msg.netId = netId;
        msg.ownerClient = ownerClient;
        msg.prefab = prefab;
        msg.position = position;
        msg.rotation = rotation;

        m_Server->Broadcast([&](net::StreamWriter &w) {
            msg.Write(w);
        }, true);

        return entity;
    }

    void NetServerReplication::DespawnEntity(uint64_t netId)
    {
        if (!m_Scene || !m_Server)
            return;

        Entity entity = m_Scene->FindEntityByNetId(netId);
        if (entity.IsValid())
        {
            SceneManager::DestroyEntity(m_Scene, entity);
        }

        netproto::DespawnMsg msg;
        msg.netId = netId;
        m_Server->Broadcast([&](net::StreamWriter &w) {
            msg.Write(w);
        }, true);
    }

    void NetServerReplication::SyncWorldToClient(uint32_t clientId)
    {
        if (!m_Scene || !m_Server)
            return;

        auto view = m_Scene->registry->view<NetworkIdentityComponent, TransformComponent>();
        for (entt::entity e : view)
        {
            const auto &[netComp, tr] = view.get<NetworkIdentityComponent, TransformComponent>(e);
            netproto::SpawnMsg msg;
            msg.netId = netComp.netId;
            msg.ownerClient = netComp.ownerClient;
            msg.prefab = netComp.prefab;
            msg.position = tr.world.translation;
            msg.rotation = tr.world.rotation;

            m_Server->SendTo(clientId, [&](net::StreamWriter &w) {
                msg.Write(w);
            }, true);
        }
    }

    void NetServerReplication::BroadcastSnapshots()
    {
        if (!m_Scene || !m_Server || m_Server->GetClientCount() == 0)
            return;

        auto view = m_Scene->registry->view<NetworkIdentityComponent, TransformComponent>();
        std::vector<netproto::EntityState> states;
        states.reserve(view.size_hint());

        for (entt::entity e : view)
        {
            const auto &[netComp, tr] = view.get<NetworkIdentityComponent, TransformComponent>(e);
            if (!netComp.replicate)
                continue;

            netproto::EntityState st;
            st.netId = netComp.netId;
            st.position = tr.world.translation;
            st.rotation = tr.world.rotation;
            st.scale = tr.world.scale;
            states.push_back(st);
        }

        // Each client receives the snapshot with its acknowledged input sequence
        m_Server->ForEachClient([&](NetServerSystem::RemoteClient &client) {
            netproto::SnapshotMsg snap;
            snap.serverTick = m_Server->GetTick();
            snap.lastProcessedInputSeq = client.lastInputSeq;
            snap.entities = states;

            m_Server->SendTo(client.clientId, [&](net::StreamWriter &w) {
                snap.Write(w);
            }, false); // snapshots are unreliable
        });
    }

    // =========================================================================
    // Client-Side Replication Implementation
    // =========================================================================
    NetClientReplication::NetClientReplication(Scene *scene, NetClientSystem *clientSystem)
        : m_Scene(scene), m_Client(clientSystem)
    {
        if (m_Scene)
        {
            m_Scene->SetNetMode(ENetMode::Client);
        }

        if (m_Client)
        {
            m_Client->onSpawn = [this](const netproto::SpawnMsg &msg) { OnSpawn(msg); };
            m_Client->onDespawn = [this](const netproto::DespawnMsg &msg) { OnDespawn(msg); };
            m_Client->onSnapshot = [this](const netproto::SnapshotMsg &msg) { OnSnapshot(msg); };
        }
    }

    NetClientReplication::~NetClientReplication()
    {
    }

    void NetClientReplication::OnSpawn(const netproto::SpawnMsg &msg)
    {
        if (!m_Scene || !m_Client)
            return;

        // If entity already exists, update it
        Entity existing = m_Scene->FindEntityByNetId(msg.netId);
        if (existing.IsValid())
            return;

        const bool isLocal = (msg.ownerClient != 0 && msg.ownerClient == m_Client->GetClientId());

        Entity entity;
        if (m_CustomSpawner)
        {
            entity = m_CustomSpawner(m_Scene, msg);
        }
        else
        {
            const std::string name = msg.prefab.empty() ? ("NetEntity_" + std::to_string(msg.netId)) : msg.prefab;
            entity = SceneManager::CreateEntity(m_Scene, name, EntityType_Node);
        }

        if (!entity.IsValid())
            return;

        auto &tr = entity.GetTransform();
        tr.local.translation = msg.position;
        tr.local.rotation = msg.rotation;
        tr.world.translation = msg.position;
        tr.world.rotation = msg.rotation;

        if (!entity.HasComponent<NetworkIdentityComponent>())
        {
            entity.AddComponent<NetworkIdentityComponent>(msg.netId, msg.ownerClient, isLocal, msg.prefab);
        }
        else
        {
            auto &netComp = entity.GetComponent<NetworkIdentityComponent>();
            netComp.netId = msg.netId;
            netComp.ownerClient = msg.ownerClient;
            netComp.isLocalPlayer = isLocal;
            netComp.prefab = msg.prefab;
            m_Scene->RegisterNetEntity(msg.netId, entity);
        }
    }

    void NetClientReplication::OnDespawn(const netproto::DespawnMsg &msg)
    {
        if (!m_Scene)
            return;

        Entity entity = m_Scene->FindEntityByNetId(msg.netId);
        if (entity.IsValid())
        {
            SceneManager::DestroyEntity(m_Scene, entity);
        }

        m_Interpolators.erase(msg.netId);
    }

    void NetClientReplication::OnSnapshot(const netproto::SnapshotMsg &msg)
    {
        if (!m_Scene || !m_Client)
            return;

        for (const auto &st : msg.entities)
        {
            Entity entity = m_Scene->FindEntityByNetId(st.netId);
            if (!entity.IsValid() || !entity.HasComponent<NetworkIdentityComponent>())
                continue;

            const auto &netComp = entity.GetComponent<NetworkIdentityComponent>();
            if (netComp.isLocalPlayer)
            {
                // Local player: reconcile prediction
                glm::vec3 correctedPos;
                glm::quat correctedRot;
                if (m_Prediction.Reconcile(msg.lastProcessedInputSeq, st.position, st.rotation,
                                           1.0f / 60.0f, nullptr, correctedPos, correctedRot))
                {
                    auto &tr = entity.GetTransform();
                    tr.local.translation = correctedPos;
                    tr.local.rotation = correctedRot;
                    tr.world.translation = correctedPos;
                    tr.world.rotation = correctedRot;
                }
            }
            else
            {
                // Remote entity: buffer into interpolator
                m_Interpolators[st.netId].AddSnapshot(msg.serverTick, m_CurrentTime, st.position, st.rotation, st.scale);
            }
        }
    }

    void NetClientReplication::Update(float deltaTime)
    {
        m_CurrentTime += deltaTime;
        m_Prediction.UpdateVisualOffset(deltaTime);

        if (!m_Scene)
            return;

        // Apply interpolation samples to remote entities
        for (auto &[netId, interp] : m_Interpolators)
        {
            Entity entity = m_Scene->FindEntityByNetId(netId);
            if (!entity.IsValid())
                continue;

            glm::vec3 pos, scale;
            glm::quat rot;
            if (interp.Sample(m_CurrentTime, pos, rot, scale))
            {
                auto &tr = entity.GetTransform();
                tr.local.translation = pos;
                tr.local.rotation = rot;
                tr.local.scale = scale;
                tr.world.translation = pos;
                tr.world.rotation = rot;
                tr.world.scale = scale;
            }
        }
    }

    Entity NetClientReplication::GetLocalPlayerEntity() const
    {
        if (!m_Scene)
            return {};

        auto view = m_Scene->registry->view<NetworkIdentityComponent>();
        for (entt::entity e : view)
        {
            const auto &comp = view.get<NetworkIdentityComponent>(e);
            if (comp.isLocalPlayer)
                return Entity{ e, m_Scene };
        }
        return {};
    }
}

