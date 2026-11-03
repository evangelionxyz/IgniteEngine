// Copyright (c) 2026 Evangelion Manuhutu

#include "net_replication.hpp"
#include <ignite/scene/scene_manager.hpp>
#include <ignite/scene/prefab.hpp>
#include <ignite/asset/asset_manager.hpp>
#include <ignite/scene/entity.hpp>
#include <ignite/scene/component.hpp>
#include <ignite/math/transform.hpp>

namespace ignite
{
    // Helper to apply world transform accounting for parent hierarchy
    static void ApplyEntityTransform(Scene *scene, Entity entity, const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scale = glm::vec3(1.0f))
    {
        auto &tr = entity.GetTransform();
        if (entity.HasComponent<IDComponent>())
        {
            auto &idc = entity.GetComponent<IDComponent>();
            if (idc.parent != 0)
            {
                Entity parent = SceneManager::GetEntity(scene, idc.parent);
                if (parent.IsValid() && parent.HasComponent<TransformComponent>())
                {
                    const auto &parentTr = parent.GetComponent<TransformComponent>();
                    const glm::mat4 childWorldMatrix = glm::translate(glm::mat4(1.0f), pos)
                        * glm::toMat4(rot) * glm::scale(glm::mat4(1.0f), scale);
                    const glm::mat4 childLocalMatrix = glm::inverse(parentTr.world.GetMatrix()) * childWorldMatrix;
                    const glm::vec3 savedLocalScale = tr.local.scale;
                    Transform::Decompose(childLocalMatrix, tr.local);
                    tr.local.scale = savedLocalScale;
                    tr.world.translation = pos;
                    tr.world.rotation = rot;
                    tr.world.scale = scale;
                    return;
                }
            }
        }

        tr.local.translation = pos;
        tr.local.rotation = rot;
        tr.local.scale = scale;
        tr.world.translation = pos;
        tr.world.rotation = rot;
        tr.world.scale = scale;
    }

    // =========================================================================
    // Server-Side Replication Implementation
    // =========================================================================
    NetServerReplication::NetServerReplication(Scene *scene, NetServerSystem *serverSystem)
        : m_Scene(scene), m_Server(serverSystem)
    {
        if (m_Scene)
        {
            m_Scene->SetNetMode(ENetMode::DedicatedServer);

            // Register existing pre-placed scene entities and ensure server generated NetIds won't collide
            uint64_t maxId = 1000;
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                auto &netComp = view.get<NetworkIdentityComponent>(e);
                if (netComp.netId != 0)
                {
                    m_Scene->RegisterNetEntity(netComp.netId, Entity{ e, m_Scene });
                    if (netComp.netId >= maxId)
                        maxId = netComp.netId;
                }
            }
            m_NextNetId = maxId + 1;
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
        Entity entity;

        if (!prefab.empty() && m_Scene->GetAssetManager())
        {
            AssetManager *am = m_Scene->GetAssetManager();
            for (const auto &[handle, metadata] : am->GetAssetAssetRegistry())
            {
                if (metadata.type == AssetType::Prefab && metadata.filepath.stem().string() == prefab)
                {
                    Ref<Prefab> p = am->GetAsset<Prefab>(handle);
                    if (p)
                    {
                        entity = SceneManager::InstantiatePrefab(m_Scene, p);
                        break;
                    }
                }
            }
        }

        if (!entity.IsValid())
        {
            const std::string name = prefab.empty() ? ("NetEntity_" + std::to_string(netId)) : prefab;
            entity = SceneManager::CreateEntity(m_Scene, name, EntityType_Node);
        }

        ApplyEntityTransform(m_Scene, entity, position, rotation);

        if (!entity.HasComponent<NetworkIdentityComponent>())
        {
            entity.AddComponent<NetworkIdentityComponent>(netId, ownerClient, false, prefab);
        }
        else
        {
            auto &netComp = entity.GetComponent<NetworkIdentityComponent>();
            netComp.netId = netId;
            netComp.ownerClient = ownerClient;
            netComp.isLocalPlayer = false;
            netComp.prefab = prefab;
        }

        m_Scene->RegisterNetEntity(netId, entity);

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
            m_Scene->UnregisterNetEntity(netId);
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

            // Synchronize attached components
            netproto::ComponentSyncMsg compMsg;
            m_Scene->SerializeEntityComponents(Entity{ e, m_Scene }, netComp.syncComponentsMask, compMsg);
            if (compMsg.compMask != 0)
            {
                m_Server->SendTo(clientId, [&](net::StreamWriter &w) {
                    compMsg.Write(w);
                }, true);
            }
        }
    }

    void NetServerReplication::BroadcastDirtyComponents()
    {
        if (!m_Scene || !m_Server || m_Server->GetClientCount() == 0)
            return;

        auto view = m_Scene->registry->view<NetworkIdentityComponent>();
        for (entt::entity e : view)
        {
            auto &netComp = view.get<NetworkIdentityComponent>(e);
            if (!netComp.replicate || netComp.dirtyComponentsMask == 0)
                continue;

            netproto::ComponentSyncMsg compMsg;
            m_Scene->SerializeEntityComponents(Entity{ e, m_Scene }, netComp.dirtyComponentsMask & netComp.syncComponentsMask, compMsg);
            netComp.ClearDirty();

            if (compMsg.compMask != 0)
            {
                m_Server->Broadcast([&](net::StreamWriter &w) {
                    compMsg.Write(w);
                }, true);
            }
        }
    }

    void NetServerReplication::BroadcastComponentSync(uint64_t netId, uint32_t compMask)
    {
        if (!m_Scene || !m_Server || m_Server->GetClientCount() == 0)
            return;

        Entity entity = m_Scene->FindEntityByNetId(netId);
        if (!entity.IsValid())
            return;

        netproto::ComponentSyncMsg compMsg;
        m_Scene->SerializeEntityComponents(entity, compMask, compMsg);
        if (compMsg.compMask != 0)
        {
            m_Server->Broadcast([&](net::StreamWriter &w) {
                compMsg.Write(w);
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

            // Scan and register existing pre-placed scene entities
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                auto &netComp = view.get<NetworkIdentityComponent>(e);
                if (netComp.netId != 0)
                {
                    m_Scene->RegisterNetEntity(netComp.netId, Entity{ e, m_Scene });
                }
            }
        }

        if (m_Client)
        {
            m_Client->onSpawn = [this](const netproto::SpawnMsg &msg) { OnSpawn(msg); };
            m_Client->onDespawn = [this](const netproto::DespawnMsg &msg) { OnDespawn(msg); };
            m_Client->onSnapshot = [this](const netproto::SnapshotMsg &msg) { OnSnapshot(msg); };
            m_Client->onComponentSync = [this](const netproto::ComponentSyncMsg &msg) { OnComponentSync(msg); };
        }
    }

    NetClientReplication::~NetClientReplication()
    {
    }

    void NetClientReplication::OnSpawn(const netproto::SpawnMsg &msg)
    {
        if (!m_Scene || !m_Client)
            return;

        const bool isLocal = (msg.ownerClient != 0 && msg.ownerClient == m_Client->GetClientId());

        // 1. If entity already exists in the scene (e.g. pre-placed mesh entity), bind to it!
        Entity existing = m_Scene->FindEntityByNetId(msg.netId);
        if (existing.IsValid())
        {
            if (existing.HasComponent<NetworkIdentityComponent>())
            {
                auto &netComp = existing.GetComponent<NetworkIdentityComponent>();
                netComp.ownerClient = msg.ownerClient;
                netComp.isLocalPlayer = isLocal;
                if (!msg.prefab.empty())
                    netComp.prefab = msg.prefab;
            }
            m_Scene->RegisterNetEntity(msg.netId, existing);

            // Snap initial transform
            ApplyEntityTransform(m_Scene, existing, msg.position, msg.rotation);
            return;
        }

        // 2. Entity does not exist yet (dynamically spawned at runtime)
        Entity entity;
        if (m_CustomSpawner)
        {
            entity = m_CustomSpawner(m_Scene, msg);
        }
        else
        {
            // Try instantiating prefab if specified
            if (!msg.prefab.empty() && m_Scene->GetAssetManager())
            {
                AssetManager *am = m_Scene->GetAssetManager();
                for (const auto &[handle, metadata] : am->GetAssetAssetRegistry())
                {
                    if (metadata.type == AssetType::Prefab && metadata.filepath.stem().string() == msg.prefab)
                    {
                        Ref<Prefab> prefab = am->GetAsset<Prefab>(handle);
                        if (prefab)
                        {
                            entity = SceneManager::InstantiatePrefab(m_Scene, prefab);
                            break;
                        }
                    }
                }
            }

            if (!entity.IsValid())
            {
                const std::string name = msg.prefab.empty() ? ("NetEntity_" + std::to_string(msg.netId)) : msg.prefab;
                entity = SceneManager::CreateEntity(m_Scene, name, EntityType_Node);
            }
        }

        if (!entity.IsValid())
            return;

        ApplyEntityTransform(m_Scene, entity, msg.position, msg.rotation);

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
        }

        m_Scene->RegisterNetEntity(msg.netId, entity);
    }

    void NetClientReplication::OnDespawn(const netproto::DespawnMsg &msg)
    {
        if (!m_Scene)
            return;

        Entity entity = m_Scene->FindEntityByNetId(msg.netId);
        if (entity.IsValid())
        {
            m_Scene->UnregisterNetEntity(msg.netId);
            SceneManager::DestroyEntity(m_Scene, entity);
        }

        m_Interpolators.erase(msg.netId);
    }

    void NetClientReplication::OnComponentSync(const netproto::ComponentSyncMsg &msg)
    {
        if (!m_Scene)
            return;

        Entity entity = m_Scene->FindEntityByNetId(msg.netId);
        if (entity.IsValid())
        {
            m_Scene->ApplyEntityComponentSync(entity, msg);
        }
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
                    ApplyEntityTransform(m_Scene, entity, correctedPos, correctedRot, st.scale);
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
                ApplyEntityTransform(m_Scene, entity, pos, rot, scale);
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
