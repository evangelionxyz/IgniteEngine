// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_NET_REPLICATION_HPP
#define IGN_NET_REPLICATION_HPP

#include "net_protocol.hpp"
#include <ignite/core/base.hpp>
#include "net_server_system.hpp"
#include "net_client_system.hpp"
#include "net_interpolator.hpp"
#include "net_prediction.hpp"

#include <ignite/scene/scene.hpp>
#include <ignite/scene/entity.hpp>
#include <ignite/scene/component.hpp>

#include <unordered_map>
#include <memory>
#include <string>

namespace ignite
{
    // =========================================================================
    // Server-Side Replication
    // =========================================================================
    class IGN_API NetServerReplication
    {
    public:
        NetServerReplication(Scene *scene, NetServerSystem *serverSystem);
        ~NetServerReplication();

        // Spawn a replicated entity and broadcast SpawnMsg to all connected clients
        Entity SpawnEntity(const std::string &prefab, const glm::vec3 &position, const glm::quat &rotation, uint32_t ownerClient = 0);

        // Despawn an entity and broadcast DespawnMsg
        void DespawnEntity(uint64_t netId);

        // Send all currently existing replicated entities to a newly joined client
        void SyncWorldToClient(uint32_t clientId);

        // Build and broadcast authoritative entity transform snapshots to clients
        void BroadcastSnapshots();

        uint64_t GenerateNetId() { return m_NextNetId++; }

    private:
        Scene *m_Scene = nullptr;
        NetServerSystem *m_Server = nullptr;
        uint64_t m_NextNetId = 1000;
    };

    // =========================================================================
    // Client-Side Replication
    // =========================================================================
    class IGN_API NetClientReplication
    {
    public:
        using EntityCustomSpawner = std::function<Entity(Scene *scene, const netproto::SpawnMsg &msg)>;

    public:
        NetClientReplication(Scene *scene, NetClientSystem *clientSystem);
        ~NetClientReplication();

        // Update interpolation for remote entities and visual offset decay
        void Update(float deltaTime);

        ClientPrediction &GetPrediction() { return m_Prediction; }
        const ClientPrediction &GetPrediction() const { return m_Prediction; }

        Entity GetLocalPlayerEntity() const;

        void SetCustomSpawner(EntityCustomSpawner spawner) { m_CustomSpawner = std::move(spawner); }

    private:
        void OnSpawn(const netproto::SpawnMsg &msg);
        void OnDespawn(const netproto::DespawnMsg &msg);
        void OnSnapshot(const netproto::SnapshotMsg &msg);

    private:
        Scene *m_Scene = nullptr;
        NetClientSystem *m_Client = nullptr;
        ClientPrediction m_Prediction;
        std::unordered_map<uint64_t, NetworkTransformInterpolator> m_Interpolators;
        EntityCustomSpawner m_CustomSpawner;
        double m_CurrentTime = 0.0;
    };
}

#endif

