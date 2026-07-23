// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "scene_serializer.hpp"
#include "entity_serializer.hpp"
#include "ignite/scene/component.hpp"

#include "ignite/scripting/script_class.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/core/application.hpp"

#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene_manager.hpp"

namespace ignite
{
    SceneSerializer::SceneSerializer(const Ref<Scene> &scene, Project *project)
        : m_Scene(scene), m_Project(project)
    {
    }

    bool SceneSerializer::Serialize(const ignite::Path &filepath)
    {
        if (!m_Scene || !m_Project)
            return false;

        Serializer sr(filepath);

        sr.BeginMap(); // START

        sr.BeginMap("Scene"); // scene file header
        sr.AddKeyValue<uint32_t>("Version", Application::GetVersion());
        sr.BeginSequence("Entities");

        // Sort by name first: copy map contents into a vector for sorting
        std::vector<std::pair<UUID, entt::entity>> sortedEntities(m_Scene->entities.begin(), m_Scene->entities.end());
        std::sort(sortedEntities.begin(), sortedEntities.end(), [scene = m_Scene](const auto &a, const auto &b)
        {
            Entity entityA = { a.second, scene.get() };
            Entity entityB = { b.second, scene.get() };

            // Use Entity::GetName() to compare display names
            const std::string &nameA = entityA.GetName();
            const std::string &nameB = entityB.GetName();
            return nameA < nameB;
        });

        // entities sequence
        for (const entt::entity e : sortedEntities | std::views::values)
        {
            Entity entity = { e, m_Scene.get() };
            const IDComponent &idComp = entity.GetComponent<IDComponent>();

            const bool isPrefab = idComp.IsInType(EntityType_Prefab);

            if (isPrefab)
                continue;

            EntitySerializer::SerializeEntity(sr, entity);
        }

        sr.EndSequence(); // Entities
        sr.EndMap(); // scene

        sr.EndMap(); // END

#if 0
        // Example
        sr.BeginMap(); // START

        sr.BeginMap("Scene"); // scene file header

        sr.AddKeyValue<std::string>("Title", m_Scene->name);
        sr.AddKeyValue<std::string>("Version", ENGINE_VERSION);

        sr.BeginSequence("Entities");


        // entities sequence
        for (int i = 0; i < 10; ++i)
        {
            sr.BeginMap(); // START Entity
            {
                sr.AddKeyValue<std::string>("ID", "ENTITY_ID");
                sr.AddKeyValue<std::string>("Type", "ENTITY_TYPE");
                sr.AddKeyValue<std::string>("Parent", "PARENT_ID");
                sr.BeginMap("Component A");
                {
                    sr.AddKeyValue("Var A", "value");
                    sr.AddKeyValue("Var B", "value");

                    sr.BeginSequence("List Var");
                    {
                        sr.BeginMap();
                        {
                            sr.AddKeyValue("List Var A", "value");
                            sr.AddKeyValue("List Var B", "value");
                        }
                        sr.EndMap();
                    }
                    sr.EndSequence();
                }
                sr.EndMap();
            }
            sr.EndMap(); // END Entity
        }

        sr.EndSequence(); // Entities
        sr.EndMap(); // scene

        sr.EndMap(); // END

#endif
        sr.Serialize();

        // Scene should be not dirty
        m_Scene->SetDirtyFlag(false);

        return true;
    }

    Ref<Scene> SceneSerializer::Deserialize(const ignite::Path &filepath, Project *project)
    {
        if (!ignite::Path::exists(filepath))
        {
            LOG_ERROR("[Scene SR] File does not exists!\n{}", filepath.generic_string());
            return nullptr;
        }

        LOG_ASSERT(project, "[Scene SR] Invalid project");

        YAML::Node sceneFileNode = Serializer::Deserialize(filepath);
        YAML::Node sceneNode = sceneFileNode["Scene"];

        LOG_ASSERT(sceneNode, "[Scene SR] Invalid scene file");
        if (!sceneNode)
            return nullptr;

        Ref<Scene> desScene = Scene::Create(project);

        // Open commandlist for asset deserialization
        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();

        for (YAML::Node entityNode : sceneNode["Entities"])
        {
            EntitySerializer::DeserializeEntity(entityNode, desScene.get(), project);
        }

        // attach each node to it's parent
        for (auto &[uuid, e] : desScene->entities)
        {
            Entity entity { e, desScene.get() };

            if (entity.GetParentUUID() != UUID(0))
            {
                Entity parent = SceneManager::GetEntity(desScene.get(), entity.GetParentUUID());
                SceneManager::AddChild(desScene.get(), parent, entity);
            }
        }

        desScene->SetDirtyFlag(false);
        return desScene;
    }
}
