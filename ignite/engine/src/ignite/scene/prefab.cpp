// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "prefab.hpp"
#include "scene_manager.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/serializer/entity_serializer.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    Prefab::Prefab(Project *project)
    {
        m_PrefabScene = Scene::Create(project);
    }

    bool Prefab::Serialize(const std::filesystem::path &filepath)
    {
        if (!m_PrefabScene)
            return false;

        Serializer sr(filepath);
        sr.BeginMap(); // START
        sr.BeginMap("Prefab");
        {
            sr.AddKeyValue("RootEntity", m_RootEntityUUID);
            sr.BeginSequence("Entities");
            {
                SceneManager::SyncHierarchyOrder(m_PrefabScene.get());
                std::unordered_set<UUID> serializedUUIDs;
                std::function<void(Entity)> serializeTree = [&](Entity entity)
                {
                    if (!entity.IsValid())
                        return;

                    const IDComponent &idComp = entity.GetComponent<IDComponent>();
                    if (serializedUUIDs.contains(idComp.uuid))
                        return;

                    serializedUUIDs.insert(idComp.uuid);
                    EntitySerializer::SerializeEntity(sr, entity);

                    for (UUID childUUID : idComp.children)
                    {
                        serializeTree(SceneManager::GetEntity(m_PrefabScene.get(), childUUID));
                    }
                };

                for (UUID rootUUID : m_PrefabScene->rootEntityOrder)
                {
                    serializeTree(SceneManager::GetEntity(m_PrefabScene.get(), rootUUID));
                }

                for (const auto &[uuid, entityHandle] : m_PrefabScene->entities)
                {
                    if (!serializedUUIDs.contains(uuid))
                    {
                        serializeTree(Entity { entityHandle, m_PrefabScene.get() });
                    }
                }
            }
            sr.EndSequence();
        }
        sr.EndMap(); // END Prefab
        sr.EndMap(); // END

        sr.Serialize();

        m_PrefabScene->SetDirtyFlag(false); // Prefab Scene
        SetDirtyFlag(false); // Prefab
        return true;
    }

    Ref<Prefab> Prefab::Deserialize(const std::filesystem::path &filepath, Project *project)
    {
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[Prefab] File does not exist: {}", filepath.generic_string());
            return nullptr;
        }

        JsonNode fileNode = Serializer::Deserialize(filepath);
        JsonNode prefabNode = fileNode["Prefab"];
        if (!prefabNode)
        {
            LOG_ERROR("[Prefab] Invalid prefab file format: {}", filepath.generic_string());
            return nullptr;
        }

        Ref<Prefab> prefab = CreateRef<Prefab>(project);
        if (prefabNode["RootEntity"])
        {
            prefab->m_RootEntityUUID = UUID(prefabNode["RootEntity"].as<uint64_t>());
        }

        if (JsonNode entitiesNode = prefabNode["Entities"])
        {
            std::unordered_map<UUID, uint32_t> orderMap;
            uint32_t seqIdx = 0;
            for (JsonNode entityNode : entitiesNode)
            {
                Entity desEntity = EntitySerializer::DeserializeEntity(entityNode, prefab->m_PrefabScene.get(), project);
                if (desEntity.IsValid())
                {
                    const uint32_t orderVal = entityNode["Order"] ? entityNode["Order"].as<uint32_t>(seqIdx) : seqIdx;
                    orderMap[desEntity.GetUUID()] = orderVal;
                }
                ++seqIdx;
            }

            prefab->m_PrefabScene->rootEntityOrder.clear();
            for (const auto &[uuid, handle] : prefab->m_PrefabScene->entities)
            {
                Entity entity { handle, prefab->m_PrefabScene.get() };
                entity.GetComponent<IDComponent>().children.clear();
            }

            // Wire up parent/child relationships
            for (auto &[uuid, handle] : prefab->m_PrefabScene->entities)
            {
                Entity entity { handle, prefab->m_PrefabScene.get() };
                if (entity.GetParentUUID() != UUID(0))
                {
                    Entity parent = SceneManager::GetEntity(prefab->m_PrefabScene.get(), entity.GetParentUUID());
                    if (parent.IsValid())
                    {
                        parent.GetComponent<IDComponent>().AddChild(uuid);
                    }
                    else
                    {
                        entity.GetComponent<IDComponent>().parent = UUID(0);
                        prefab->m_PrefabScene->rootEntityOrder.push_back(uuid);
                    }
                }
                else
                {
                    prefab->m_PrefabScene->rootEntityOrder.push_back(uuid);
                }
            }

            auto sortByOrder = [&orderMap](std::vector<UUID> &list)
            {
                std::stable_sort(list.begin(), list.end(), [&orderMap](UUID a, UUID b)
                {
                    const auto itA = orderMap.find(a);
                    const auto itB = orderMap.find(b);
                    const uint32_t ordA = itA != orderMap.end() ? itA->second : (std::numeric_limits<uint32_t>::max)();
                    const uint32_t ordB = itB != orderMap.end() ? itB->second : (std::numeric_limits<uint32_t>::max)();
                    return ordA < ordB;
                });
            };

            sortByOrder(prefab->m_PrefabScene->rootEntityOrder);
            for (const auto &[uuid, handle] : prefab->m_PrefabScene->entities)
            {
                Entity entity { handle, prefab->m_PrefabScene.get() };
                sortByOrder(entity.GetComponent<IDComponent>().children);
            }
            SceneManager::SyncHierarchyOrder(prefab->m_PrefabScene.get());
        }

		prefab->SetDirtyFlag(false); // Prefab
        prefab->GetPrefabScene()->SetDirtyFlag(false); // Prefab scene
        return prefab;
    }

    Ref<Prefab> Prefab::CreateFromEntity(Entity entity, Scene *scene, Project *project)
    {
        if (!entity.IsValid() || !scene)
            return nullptr;

        Ref<Prefab> prefab = CreateRef<Prefab>(project);
        Entity clonedRoot = SceneManager::CloneEntityTree(prefab->m_PrefabScene.get(), scene, entity);
        if (clonedRoot.IsValid())
        {
            if (clonedRoot.HasComponent<PrefabComponent>())
                clonedRoot.RemoveComponent<PrefabComponent>();

            prefab->m_RootEntityUUID = clonedRoot.GetUUID();
            // Reset world position of cloned root relative to origin for clean prefab editing
            auto &tr = clonedRoot.GetComponent<TransformComponent>();
            tr.local.translation = glm::vec3(0.0f);
            tr.local.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            tr.local.scale = glm::vec3(1.0f);
            tr.world = tr.local;
        }

        return prefab;
    }

    bool Prefab::UpdateFromEntity(Entity entity, Scene *scene, Project *project)
    {
        if (!entity.IsValid() || !scene)
            return false;

        m_PrefabScene = Scene::Create(project);
        Entity clonedRoot = SceneManager::CloneEntityTree(m_PrefabScene.get(), scene, entity);
        if (clonedRoot.IsValid())
        {
            if (clonedRoot.HasComponent<PrefabComponent>())
                clonedRoot.RemoveComponent<PrefabComponent>();

            m_RootEntityUUID = clonedRoot.GetUUID();
            auto &tr = clonedRoot.GetComponent<TransformComponent>();
            tr.local.translation = glm::vec3(0.0f);
            tr.local.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            tr.local.scale = glm::vec3(1.0f);
            tr.world = tr.local;
            return true;
        }

        return false;
    }

    Entity Prefab::Instantiate(const Ref<Prefab> &prefab, Scene *targetScene)
    {
        return SceneManager::InstantiatePrefab(targetScene, prefab);
    }
}
