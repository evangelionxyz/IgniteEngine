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
                for (const auto &[uuid, entityHandle] : m_PrefabScene->entities)
                {
                    Entity entity = { entityHandle, m_PrefabScene.get() };
                    if (entity.IsValid())
                    {
                        EntitySerializer::SerializeEntity(sr, entity);
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

        YAML::Node fileNode = Serializer::Deserialize(filepath);
        YAML::Node prefabNode = fileNode["Prefab"];
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

        if (YAML::Node entitiesNode = prefabNode["Entities"])
        {
            for (YAML::Node entityNode : entitiesNode)
            {
                EntitySerializer::DeserializeEntity(entityNode, prefab->m_PrefabScene.get(), project);
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
                        SceneManager::AddChild(prefab->m_PrefabScene.get(), parent, entity);
                    }
                }
            }
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
