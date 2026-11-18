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
    static void ApplyEntityTransform(Scene *scene, Entity entity, const glm::vec3 &pos, const glm::quat &rot, const glm::vec3 &scale)
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
                    tr.dirty = true;
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
        tr.dirty = true;

        if (entity.HasComponent<CharacterControllerComponent>())
        {
            auto &cc = entity.GetComponent<CharacterControllerComponent>();
            if (auto charCtrl = cc.character.lock())
            {
                charCtrl->SetPosition(pos);
                charCtrl->SetRotation(rot);
            }
            else
            {
                cc.dirty = true;
            }
        }
    }

    static glm::vec3 GetEntityWorldScale(Entity entity)
    {
        return entity.HasComponent<TransformComponent>()
            ? entity.GetComponent<TransformComponent>().world.scale
            : glm::vec3(1.0f);
    }

    // =========================================================================
    // Server-Side Replication Implementation
    // =========================================================================
    namespace
    {
        void SyncSpawnedCharacterPhysics(Entity entity, const glm::vec3 &pos, const glm::quat &rot)
        {
            if (!entity.IsValid())
                return;

            if (entity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = entity.GetComponent<CharacterControllerComponent>();
                cc.linearVelocity = glm::vec3(0.0f);
                cc.desiredVelocity = glm::vec3(0.0f);
                if (auto charCtrl = cc.character.lock())
                {
                    charCtrl->SetPosition(pos);
                    charCtrl->SetRotation(rot);
                    charCtrl->SetLinearVelocity(glm::vec3(0.0f));
                }
                else
                {
                    cc.dirty = true;
                }
            }
        }

        void SetCamerasPrimaryRecursive(Scene *scene, Entity entity, bool primary)
        {
            if (!scene || !entity.IsValid())
                return;

            if (entity.HasComponent<CameraComponent>())
            {
                entity.GetComponent<CameraComponent>().primary = primary;
            }

            if (entity.HasComponent<IDComponent>())
            {
                const auto &children = entity.GetComponent<IDComponent>().children;
                for (UUID childUUID : children)
                {
                    SetCamerasPrimaryRecursive(scene, SceneManager::GetEntity(scene, childUUID), primary);
                }
            }
        }
    }

    NetServerReplication::NetServerReplication(Scene *scene, NetServerSystem *serverSystem)
        : m_Scene(scene), m_Server(serverSystem)
    {
        if (m_Scene)
        {
            m_Scene->SetNetMode(ENetMode::DedicatedServer);

            // Scene files contain player roles, not runtime network identifiers.
            m_NextNetId = 1000;
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                auto &netComp = view.get<NetworkIdentityComponent>(e);
                if (netComp.netId != 0)
                    m_Scene->UnregisterNetEntity(netComp.netId);
                netComp.netId = 0;
                netComp.ownerClient = 0;

                if (netComp.isLocalPlayer)
                {
                    netComp.replicate = true;
                    if (netComp.prefab.empty())
                        netComp.prefab = Entity{ e, m_Scene }.GetName();
                }
                if (!netComp.replicate)
                    continue;

                netComp.netId = GenerateNetId();
                m_Scene->RegisterNetEntity(netComp.netId, Entity{ e, m_Scene });
            }
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

        if (!entity.IsValid() && !prefab.empty())
        {
            auto view = m_Scene->registry->view<IDComponent>();
            for (entt::entity candidate : view)
            {
                Entity templateEntity(candidate, m_Scene);
                if (templateEntity.GetName() == prefab)
                {
                    entity = SceneManager::DuplicateEntity(m_Scene, templateEntity);
                    if (entity.IsValid())
                    {
                        SceneManager::RenameEntity(m_Scene, entity, "Player_" + std::to_string(ownerClient));
                    }
                    break;
                }
            }
        }

        if (!entity.IsValid())
        {
            const std::string name = prefab.empty() ? ("NetEntity_" + std::to_string(netId)) : prefab;
            entity = SceneManager::CreateEntity(m_Scene, name, EntityType_Node);
        }

        ApplyEntityTransform(m_Scene, entity, position, rotation, GetEntityWorldScale(entity));
        SyncSpawnedCharacterPhysics(entity, position, rotation);

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
            netComp.replicate = true;
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

        m_Server->Broadcast([&](net::StreamWriter &w)
        {
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
        m_Server->Broadcast([&](net::StreamWriter &w)
        {
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
            if (!netComp.replicate || netComp.netId == 0)
                continue;

            netproto::SpawnMsg msg;
            msg.netId = netComp.netId;
            msg.ownerClient = netComp.ownerClient;
            msg.prefab = netComp.prefab;

            msg.position = tr.world.translation;
            msg.rotation = tr.world.rotation;

            m_Server->SendTo(clientId, [&](net::StreamWriter &w)
            {
                msg.Write(w);
            }, true);

            // Synchronize attached components
            netproto::ComponentSyncMsg compMsg;
            m_Scene->SerializeEntityComponents(Entity{ e, m_Scene }, netComp.syncComponentsMask, compMsg);
            if (compMsg.compMask != 0)
            {
                m_Server->SendTo(clientId, [&](net::StreamWriter &w)
                {
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
            if (!netComp.replicate || netComp.netId == 0 || netComp.dirtyComponentsMask == 0)
                continue;

            netproto::ComponentSyncMsg compMsg;
            m_Scene->SerializeEntityComponents(Entity{ e, m_Scene }, netComp.dirtyComponentsMask & netComp.syncComponentsMask, compMsg);
            netComp.ClearDirty();

            if (compMsg.compMask != 0)
            {
                m_Server->Broadcast([&](net::StreamWriter &w)
                {
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
            m_Server->Broadcast([&](net::StreamWriter &w)
            {
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
            if (!netComp.replicate || netComp.netId == 0)
                continue;

            netproto::EntityState st;
            st.netId = netComp.netId;
            st.position = tr.world.translation;
            st.rotation = tr.world.rotation;
            st.scale = tr.world.scale;
            if (m_Scene->registry->any_of<CharacterControllerComponent>(e))
            {
                const auto &controller = m_Scene->registry->get<CharacterControllerComponent>(e);
                glm::vec3 groundVelocity(0.0f);
                if (auto character = controller.character.lock())
                {
                    st.isGrounded = character->IsSupported();
                    if (st.isGrounded)
                        groundVelocity = character->GetGroundVelocity();
                }
                const glm::vec3 movementVelocity = controller.linearVelocity - groundVelocity;
                st.movementSpeed = glm::length(glm::vec2(movementVelocity.x, movementVelocity.z));
            }
            states.push_back(st);
        }

        // Each client receives the snapshot with its acknowledged input sequence
        m_Server->ForEachClient([&](NetServerSystem::RemoteClient &client)
        {
            netproto::SnapshotMsg snap;
            snap.serverTick = m_Server->GetTick();
            snap.lastProcessedInputSeq = client.lastInputSeq;
            snap.entities = states;

            m_Server->SendTo(client.clientId, [&](net::StreamWriter &w)
            {
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

            // Scene-authored IDs are stale by definition; IDs arrive from the server.
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity e : view)
            {
                auto &netComp = view.get<NetworkIdentityComponent>(e);
                if (netComp.netId != 0)
                {
                    m_Scene->UnregisterNetEntity(netComp.netId);
                }

                netComp.netId = 0;
                netComp.ownerClient = 0;
                if (netComp.isLocalPlayer && !m_LocalTemplate.IsValid())
                {
                    m_LocalTemplate = Entity{ e, m_Scene };
                    m_LocalTemplateInitialVisible = m_LocalTemplate.HasComponent<RenderingComponent>()
                        ? m_LocalTemplate.GetComponent<RenderingComponent>().visible
                        : true;
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
        if (m_Scene && m_LocalTemplate.IsValid())
        {
            if (m_LocalTemplate.HasComponent<RenderingComponent>())
                m_LocalTemplate.GetComponent<RenderingComponent>().visible = m_LocalTemplateInitialVisible;

            if (m_LocalTemplate.HasComponent<NetworkIdentityComponent>())
            {
                auto &netComp = m_LocalTemplate.GetComponent<NetworkIdentityComponent>();
                if (netComp.netId != 0)
                    m_Scene->UnregisterNetEntity(netComp.netId);
                netComp.netId = 0;
                netComp.ownerClient = 0;
                netComp.isLocalPlayer = true;
            }
        }
    }

    void NetClientReplication::OnSpawn(const netproto::SpawnMsg &msg)
    {
        if (!m_Scene || !m_Client)
            return;

        const bool isLocal = (msg.ownerClient != 0 && msg.ownerClient == m_Client->GetClientId());
        const auto matchesLocalTemplate = [this, &msg]()
        {
            if (!m_LocalTemplate.IsValid() || msg.prefab.empty())
                return false;

            if (msg.prefab == m_LocalTemplate.GetName())
                return true;

            return m_LocalTemplate.HasComponent<NetworkIdentityComponent>() && msg.prefab == m_LocalTemplate.GetComponent<NetworkIdentityComponent>().prefab;
        };

        const bool isPlayerAvatar = matchesLocalTemplate();

        // 1. If entity already exists in the scene (e.g. pre-placed mesh entity), bind to it!
        Entity existing = m_Scene->FindEntityByNetId(msg.netId);
        if (!existing.IsValid() && !isLocal && !msg.prefab.empty())
        {
            auto view = m_Scene->registry->view<NetworkIdentityComponent>();
            for (entt::entity candidate : view)
            {
                const auto &identity = view.get<NetworkIdentityComponent>(candidate);
                if (identity.netId != 0 || identity.isLocalPlayer || !identity.replicate)
                    continue;

                Entity authoredEntity{ candidate, m_Scene };
                if (authoredEntity.GetName() == msg.prefab || identity.prefab == msg.prefab)
                {
                    existing = authoredEntity;
                    break;
                }
            }
        }
        if (existing.IsValid())
        {
            if (existing.HasComponent<NetworkIdentityComponent>())
            {
                auto &netComp = existing.GetComponent<NetworkIdentityComponent>();
                netComp.netId = msg.netId;
                netComp.ownerClient = msg.ownerClient;
                netComp.isLocalPlayer = isLocal;
                netComp.replicate = true;
                netComp.replicatedMovementSpeed = 0.0f;
                netComp.replicatedIsGrounded = false;
                if (!msg.prefab.empty())
                    netComp.prefab = msg.prefab;
            }
            m_Scene->RegisterNetEntity(msg.netId, existing);
            if (existing.HasComponent<RenderingComponent>())
                existing.GetComponent<RenderingComponent>().visible = true;

            // Snap initial transform
            ApplyEntityTransform(m_Scene, existing, msg.position, msg.rotation, GetEntityWorldScale(existing));
            return;
        }

        // 2. Entity does not exist yet (dynamically spawned at runtime)
        Entity entity;
        if (m_CustomSpawner)
        {
            entity = m_CustomSpawner(m_Scene, msg);
        }
        else if (isLocal && !m_LocalClaimed && m_LocalTemplate.IsValid()
            && (isPlayerAvatar || msg.prefab.empty() || msg.prefab == "Player"))
        {
            // Our own avatar: claim the scene's local player entity.
            entity = m_LocalTemplate;
            m_LocalClaimed = true;
        }
        else if (isPlayerAvatar && !isLocal && m_LocalTemplate.IsValid())
        {
            // Each remote player gets an independent copy of the authored avatar.
            entity = SceneManager::DuplicateEntity(m_Scene, m_LocalTemplate);
            if (entity.IsValid())
            {
                SceneManager::RenameEntity(m_Scene, entity, "Player_" + std::to_string(msg.ownerClient));
                SetCamerasPrimaryRecursive(m_Scene, entity, false);
            }
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

        if (entity.HasComponent<RenderingComponent>())
            entity.GetComponent<RenderingComponent>().visible = true;

        ApplyEntityTransform(m_Scene, entity, msg.position, msg.rotation, GetEntityWorldScale(entity));
        SyncSpawnedCharacterPhysics(entity, msg.position, msg.rotation);
        if (isPlayerAvatar)
        {
            SetCamerasPrimaryRecursive(m_Scene, entity, isLocal);
        }

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
            netComp.replicate = true;
            netComp.prefab = msg.prefab;
            netComp.replicatedMovementSpeed = 0.0f;
            netComp.replicatedIsGrounded = false;
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
        m_PreviousInterpolatedPositions.erase(msg.netId);
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

            auto &netComp = entity.GetComponent<NetworkIdentityComponent>();
            netComp.replicatedMovementSpeed = st.movementSpeed;
            netComp.replicatedIsGrounded = st.isGrounded;
            if (netComp.isLocalPlayer)
            {
                // Physics character controllers are driven locally; don't fight them.
                if (entity.HasComponent<CharacterControllerComponent>())
                {
                    // Owner-authoritative: only intervene on gross, persistent divergence
                    // (rejected teleport, server-side respawn). Snapshots lag by RTT, so small
                    // errors are expected while moving and must not be corrected.
                    auto &controller = entity.GetComponent<CharacterControllerComponent>();
                    auto character = controller.character.lock();
                    if (!character)
                        continue;

                    const float err = glm::distance(character->GetPosition(), st.position);
                    if (err > kHardCorrectionDistance)
                    {
                        if (m_HardErrorSince < 0.0)
                            m_HardErrorSince = m_CurrentTime;
                        if (m_CurrentTime - m_HardErrorSince >= kHardCorrectionHoldSec)
                        {
                            character->SetPosition(st.position);
                            ApplyEntityTransform(m_Scene, entity, st.position, st.rotation, GetEntityWorldScale(entity));
                            m_HardErrorSince = -1.0;
                        }
                    }
                    else
                    {
                        m_HardErrorSince = -1.0;
                    }
                    continue;
                }

                // Local player: reconcile prediction and retain smooth visual error offset
                glm::vec3 correctedPos;
                glm::quat correctedRot;
                if (m_Prediction.Reconcile(msg.lastProcessedInputSeq, st.position, st.rotation,
                                           1.0f / 60.0f, nullptr, correctedPos, correctedRot))
                {
                    ApplyEntityTransform(m_Scene, entity, correctedPos + m_Prediction.GetVisualOffset(), correctedRot, st.scale);
                }
            }
            else
            {
                // Remote entity: buffer into interpolator
                const bool isNew = m_Interpolators.find(st.netId) == m_Interpolators.end();
                auto &interp = m_Interpolators[st.netId];
                if (isNew && entity.HasComponent<RigidbodyComponent>()
                    && entity.GetComponent<RigidbodyComponent>().bodyType == physics::BodyType::Kinematic)
                {
                    // Kinematic platforms carry the local player: keep them close to server time.
                    interp.SetInterpolationDelay(0.06);
                    interp.SetMaxExtrapolation(0.1);
                }
                interp.AddSnapshot(msg.serverTick, m_CurrentTime, st.position, st.rotation, st.scale);
            }
        }
    }

    void NetClientReplication::Update(float deltaTime)
    {
        m_CurrentTime += deltaTime;
        const glm::vec3 prevOffset = m_Prediction.GetVisualOffset();
        m_Prediction.UpdateVisualOffset(deltaTime);
        const glm::vec3 offsetDelta = m_Prediction.GetVisualOffset() - prevOffset;

        if (!m_Scene)
            return;

        if (glm::dot(offsetDelta, offsetDelta) > 0.0f)
        {
            Entity localPlayer = GetLocalPlayerEntity();
            if (localPlayer.IsValid() && localPlayer.HasComponent<TransformComponent>())
            {
                auto &tc = localPlayer.GetComponent<TransformComponent>();
                tc.local.translation += offsetDelta;
                tc.world.translation += offsetDelta;
                tc.dirty = true;
            }
        }

        // Apply interpolation samples to remote entities
        for (auto &[netId, interp] : m_Interpolators)
        {
            Entity entity = m_Scene->FindEntityByNetId(netId);
            if (!entity.IsValid())
                continue;

            glm::vec3 pos;
            glm::vec3 scale;
            glm::quat rot;
            if (interp.Sample(m_CurrentTime, pos, rot, scale))
            {
                if (entity.HasComponent<RigidbodyComponent>() && deltaTime > 0.0f)
                {
                    auto &rigidbody = entity.GetComponent<RigidbodyComponent>();
                    if (rigidbody.bodyType == physics::BodyType::Kinematic)
                    {
                        auto previous = m_PreviousInterpolatedPositions.find(netId);
                        if (previous != m_PreviousInterpolatedPositions.end())
                        {
                            rigidbody.linearVelocity = (pos - previous->second) / deltaTime;
                            if (auto actor = rigidbody.dynamicActor.lock())
                                actor->SetLinearVelocity(rigidbody.linearVelocity);
                        }
                    }
                }
                m_PreviousInterpolatedPositions[netId] = pos;
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
