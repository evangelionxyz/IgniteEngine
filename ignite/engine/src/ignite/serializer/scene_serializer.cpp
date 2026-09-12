// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "scene_serializer.hpp"
#include "entity_serializer.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"

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

#include <nlohmann/json.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ignite
{
    SceneSerializer::SceneSerializer(const Ref<Scene> &scene, Project *project)
        : m_Scene(scene), m_Project(project)
    {
    }

    bool SceneSerializer::Serialize(const std::filesystem::path &filepath)
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

    Ref<Scene> SceneSerializer::Deserialize(const std::filesystem::path &filepath, Project *project)
    {
        if (!std::filesystem::exists(filepath))
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

    using json = nlohmann::ordered_json;

    std::string SceneSerializer::DeserializeHierarchyJson(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[SceneSerializer] File does not exist: {}", filepath.generic_string());
            return "[]";
        }

        YAML::Node sceneFileNode;
        try
        {
            sceneFileNode = Serializer::Deserialize(filepath);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("[SceneSerializer] Failed to parse scene YAML: {}", e.what());
            return "[]";
        }

        YAML::Node sceneNode = sceneFileNode["Scene"];
        if (!sceneNode || !sceneNode["Entities"])
        {
            return "[]";
        }

        struct EntityEntry
        {
            uint64_t id = 0;
            std::string name = "Entity";
            uint64_t parent = 0;
            bool isActive = true;
            std::vector<json> components;
            std::vector<uint64_t> childIds;
        };

        std::unordered_map<uint64_t, EntityEntry> entries;
        std::vector<uint64_t> rootIds;
        std::vector<uint64_t> allIdsInOrder;

        for (YAML::Node entityNode : sceneNode["Entities"])
        {
            EntityEntry entry;
            if (entityNode["ID"])
            {
                entry.id = entityNode["ID"].as<uint64_t>(0);
            }
            if (entityNode["Name"])
            {
                entry.name = entityNode["Name"].as<std::string>("Entity");
            }
            if (entityNode["Parent"])
            {
                entry.parent = entityNode["Parent"].as<uint64_t>(0);
            }

            // 1. Transform
            if (entityNode["Transform"])
            {
                YAML::Node tNode = entityNode["Transform"];
                glm::vec3 pos = tNode["LocalTranslation"] ? tNode["LocalTranslation"].as<glm::vec3>(glm::vec3(0.0f)) :
                    (tNode["WorldTranslation"] ? tNode["WorldTranslation"].as<glm::vec3>(glm::vec3(0.0f)) : glm::vec3(0.0f));
                glm::vec3 rot(0.0f);
                if (tNode["LocalRotation"])
                {
                    try {
                        glm::quat q = tNode["LocalRotation"].as<glm::quat>();
                        rot = glm::degrees(glm::eulerAngles(q));
                    }
                    catch (...) {
                        try {
                            rot = tNode["LocalRotation"].as<glm::vec3>(glm::vec3(0.0f));
                        }
                        catch (...) {}
                    }
                }
                else if (tNode["WorldRotation"])
                {
                    try {
                        glm::quat q = tNode["WorldRotation"].as<glm::quat>();
                        rot = glm::degrees(glm::eulerAngles(q));
                    }
                    catch (...) {
                        try {
                            rot = tNode["WorldRotation"].as<glm::vec3>(glm::vec3(0.0f));
                        }
                        catch (...) {}
                    }
                }
                glm::vec3 scl = tNode["LocalScale"] ? tNode["LocalScale"].as<glm::vec3>(glm::vec3(1.0f)) :
                    (tNode["WorldScale"] ? tNode["WorldScale"].as<glm::vec3>(glm::vec3(1.0f)) : glm::vec3(1.0f));

                json c;
                c["type"] = "Transform";
                c["displayName"] = "Transform";
                c["position"] = { pos.x, pos.y, pos.z };
                c["rotation"] = { rot.x, rot.y, rot.z };
                c["scale"] = { scl.x, scl.y, scl.z };
                entry.components.push_back(c);
            }

            // 2. Camera
            if (entityNode["Camera"])
            {
                YAML::Node cNode = entityNode["Camera"];
                int proj = cNode["Projection"] ? cNode["Projection"].as<int>(1) : 1;
                float fov = cNode["Fov"] ? cNode["Fov"].as<float>(60.0f) : 60.0f;
                float nearP = cNode["Near"] ? cNode["Near"].as<float>(0.1f) : 0.1f;
                float farP = cNode["Far"] ? cNode["Far"].as<float>(1000.0f) : 1000.0f;
                float orthoSize = cNode["OrthoSize"] ? cNode["OrthoSize"].as<float>(10.0f) : 10.0f;

                json c;
                c["type"] = "Camera";
                c["displayName"] = "Camera";
                c["isPerspective"] = (proj == 1);
                c["fov"] = fov;
                c["near"] = nearP;
                c["far"] = farP;
                c["orthoSize"] = orthoSize;
                entry.components.push_back(c);
            }

            // 3. DirectionalLight
            if (entityNode["DirectionalLight"])
            {
                YAML::Node lNode = entityNode["DirectionalLight"];
                glm::vec4 color = lNode["Color"] ? lNode["Color"].as<glm::vec4>(glm::vec4(1.0f)) : glm::vec4(1.0f);
                float intensity = lNode["Intensity"] ? lNode["Intensity"].as<float>(1.0f) : 1.0f;

                json c;
                c["type"] = "DirectionalLight";
                c["displayName"] = "Directional Light";
                c["color"] = { color.r, color.g, color.b };
                c["intensity"] = intensity;
                entry.components.push_back(c);
            }

            // 4. PointLight
            if (entityNode["PointLight"])
            {
                YAML::Node lNode = entityNode["PointLight"];
                glm::vec4 color = lNode["Color"] ? lNode["Color"].as<glm::vec4>(glm::vec4(1.0f)) : glm::vec4(1.0f);
                float intensity = lNode["Intensity"] ? lNode["Intensity"].as<float>(1.0f) : 1.0f;
                float range = lNode["Range"] ? lNode["Range"].as<float>(10.0f) : 10.0f;

                json c;
                c["type"] = "PointLight";
                c["displayName"] = "Point Light";
                c["color"] = { color.r, color.g, color.b };
                c["intensity"] = intensity;
                c["range"] = range;
                entry.components.push_back(c);
            }

            // 5. SpotLight
            if (entityNode["SpotLight"])
            {
                YAML::Node lNode = entityNode["SpotLight"];
                glm::vec4 color = lNode["Color"] ? lNode["Color"].as<glm::vec4>(glm::vec4(1.0f)) : glm::vec4(1.0f);
                float intensity = lNode["Intensity"] ? lNode["Intensity"].as<float>(1.0f) : 1.0f;
                float range = lNode["Range"] ? lNode["Range"].as<float>(10.0f) : 10.0f;

                json c;
                c["type"] = "SpotLight";
                c["displayName"] = "Spot Light";
                c["color"] = { color.r, color.g, color.b };
                c["intensity"] = intensity;
                c["range"] = range;
                entry.components.push_back(c);
            }

            // 6. Sprite2D
            if (entityNode["Sprite2D"])
            {
                YAML::Node sNode = entityNode["Sprite2D"];
                glm::vec4 color = sNode["Color"] ? sNode["Color"].as<glm::vec4>(glm::vec4(1.0f)) : glm::vec4(1.0f);

                json c;
                c["type"] = "Sprite2D";
                c["displayName"] = "Sprite 2D";
                c["color"] = { color.r, color.g, color.b, color.a };
                entry.components.push_back(c);
            }

            // 7. Circle2D
            if (entityNode["Circle2D"])
            {
                YAML::Node sNode = entityNode["Circle2D"];
                glm::vec4 color = sNode["Color"] ? sNode["Color"].as<glm::vec4>(glm::vec4(1.0f)) : glm::vec4(1.0f);

                json c;
                c["type"] = "Circle2D";
                c["displayName"] = "Circle 2D";
                c["color"] = { color.r, color.g, color.b, color.a };
                entry.components.push_back(c);
            }

            // 8. StaticMesh
            if (entityNode["StaticMesh"])
            {
                json c;
                c["type"] = "StaticMesh";
                c["displayName"] = "Static Mesh";
                entry.components.push_back(c);
            }

            // 9. SkeletalMesh
            if (entityNode["SkeletalMesh"])
            {
                json c;
                c["type"] = "SkeletalMesh";
                c["displayName"] = "Skeletal Mesh";
                entry.components.push_back(c);
            }

            // 10. Rigidbody
            if (entityNode["Rigidbody"])
            {
                YAML::Node rbNode = entityNode["Rigidbody"];
                float mass = rbNode["Mass"] ? rbNode["Mass"].as<float>(1.0f) : 1.0f;

                json c;
                c["type"] = "Rigidbody";
                c["displayName"] = "Rigid Body";
                c["mass"] = mass;
                entry.components.push_back(c);
            }

            // 11. BoxCollider
            if (entityNode["BoxCollider"])
            {
                json c;
                c["type"] = "BoxCollider";
                c["displayName"] = "Box Collider";
                entry.components.push_back(c);
            }

            // 12. SphereCollider
            if (entityNode["SphereCollider"])
            {
                json c;
                c["type"] = "SphereCollider";
                c["displayName"] = "Sphere Collider";
                entry.components.push_back(c);
            }

            // 13. CapsuleCollider
            if (entityNode["CapsuleCollider"])
            {
                json c;
                c["type"] = "CapsuleCollider";
                c["displayName"] = "Capsule Collider";
                entry.components.push_back(c);
            }

            // 14. MeshCollider
            if (entityNode["MeshCollider"])
            {
                json c;
                c["type"] = "MeshCollider";
                c["displayName"] = "Mesh Collider";
                entry.components.push_back(c);
            }

            // 15. AudioSource
            if (entityNode["AudioSource"])
            {
                json c;
                c["type"] = "AudioSource";
                c["displayName"] = "Audio Source";
                entry.components.push_back(c);
            }

            // 16. Script
            if (entityNode["Script"])
            {
                json c;
                c["type"] = "Script";
                c["displayName"] = "Script";
                entry.components.push_back(c);
            }

            // 17. WorldEnvironment
            if (entityNode["WorldEnvironment"])
            {
                json c;
                c["type"] = "WorldEnvironment";
                c["displayName"] = "World Environment";
                entry.components.push_back(c);
            }

            // 18. CharacterController
            if (entityNode["CharacterController"])
            {
                json c;
                c["type"] = "CharacterController";
                c["displayName"] = "Character Controller";
                entry.components.push_back(c);
            }

            entries[entry.id] = std::move(entry);
            allIdsInOrder.push_back(entry.id);
        }

        // Build parent-child tree
        for (uint64_t id : allIdsInOrder)
        {
            auto &entry = entries[id];
            if (entry.parent != 0 && entries.find(entry.parent) != entries.end())
            {
                entries[entry.parent].childIds.push_back(id);
            }
            else
            {
                rootIds.push_back(id);
            }
        }

        std::function<json(uint64_t)> buildNode = [&](uint64_t id) -> json
            {
                const auto &e = entries[id];
                json node;
                node["id"] = std::to_string(e.id);
                node["rawId"] = e.id;
                node["name"] = e.name;
                if (e.parent != 0)
                    node["parentId"] = std::to_string(e.parent);
                else
                    node["parentId"] = nullptr;
                node["isActive"] = e.isActive;
                node["components"] = e.components;

                json children = json::array();
                for (uint64_t childId : e.childIds)
                {
                    children.push_back(buildNode(childId));
                }
                node["children"] = children;
                return node;
            };

        json rootArray = json::array();
        for (uint64_t id : rootIds)
        {
            rootArray.push_back(buildNode(id));
        }

        return rootArray.dump();
    }

    std::string SceneSerializer::GetSceneHierarchyJson(Scene *scene)
    {
        if (!scene || !scene->registry)
        {
            return "[]";
        }

        struct EntityEntry
        {
            uint64_t id = 0;
            std::string name = "Entity";
            uint64_t parent = 0;
            bool isActive = true;
            std::vector<json> components;
            std::vector<uint64_t> childIds;
        };

        std::unordered_map<uint64_t, EntityEntry> entries;
        std::vector<uint64_t> rootIds;
        std::vector<uint64_t> allIdsInOrder;

        auto view = scene->registry->view<IDComponent>();
        for (auto entityHandle : view)
        {
            Entity entity{ entityHandle, scene };
            const IDComponent &idComp = entity.GetComponent<IDComponent>();

            EntityEntry entry;
            entry.id = static_cast<uint64_t>(idComp.uuid);
            entry.name = idComp.name;
            entry.parent = static_cast<uint64_t>(idComp.parent);

            // Transform
            if (entity.HasComponent<TransformComponent>())
            {
                const auto &comp = entity.GetComponent<TransformComponent>();
                json c;
                c["type"] = "Transform";
                c["displayName"] = "Transform";
                c["position"] = { comp.local.translation.x, comp.local.translation.y, comp.local.translation.z };
                glm::vec3 eulerRot = glm::degrees(glm::eulerAngles(comp.local.rotation));
                c["rotation"] = { eulerRot.x, eulerRot.y, eulerRot.z };
                c["scale"] = { comp.local.scale.x, comp.local.scale.y, comp.local.scale.z };
                entry.components.push_back(c);
            }

            // Camera
            if (entity.HasComponent<CameraComponent>())
            {
                const auto &comp = entity.GetComponent<CameraComponent>();
                json c;
                c["type"] = "Camera";
                c["displayName"] = "Camera";
                c["isPerspective"] = (comp.camera.projectionType == ProjectionType::Perspective);
                c["fov"] = comp.camera.fov;
                c["near"] = comp.camera.nearPlane;
                c["far"] = comp.camera.farPlane;
                c["orthoSize"] = comp.camera.orthoSize;
                entry.components.push_back(c);
            }

            // Directional Light
            if (entity.HasComponent<DirectionalLightComponent>())
            {
                const auto &comp = entity.GetComponent<DirectionalLightComponent>();
                json c;
                c["type"] = "DirectionalLight";
                c["displayName"] = "Directional Light";
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["intensity"] = comp.intensity;
                c["shadowDistance"] = comp.shadowDistance;
                c["castShadows"] = comp.cascadeShadow;
                entry.components.push_back(c);
            }

            // Point Light
            if (entity.HasComponent<PointLightComponent>())
            {
                const auto &comp = entity.GetComponent<PointLightComponent>();
                json c;
                c["type"] = "PointLight";
                c["displayName"] = "Point Light";
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["intensity"] = comp.intensity;
                c["range"] = comp.range;
                c["enabled"] = comp.enabled;
                entry.components.push_back(c);
            }

            // Spot Light
            if (entity.HasComponent<SpotLightComponent>())
            {
                const auto &comp = entity.GetComponent<SpotLightComponent>();
                json c;
                c["type"] = "SpotLight";
                c["displayName"] = "Spot Light";
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["intensity"] = comp.intensity;
                c["range"] = comp.range;
                c["innerCone"] = comp.innerConeAngle;
                c["outerCone"] = comp.outerConeAngle;
                c["enabled"] = comp.enabled;
                entry.components.push_back(c);
            }

            // Point Light 2D
            if (entity.HasComponent<PointLight2DComponent>())
            {
                const auto &comp = entity.GetComponent<PointLight2DComponent>();
                json c;
                c["type"] = "PointLight2D";
                c["displayName"] = "Point Light 2D";
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["radius"] = comp.radius;
                c["intensity"] = comp.intensity;
                c["enabled"] = comp.enabled;
                entry.components.push_back(c);
            }

            // Sprite2D
            if (entity.HasComponent<Sprite2DComponent>())
            {
                const auto &comp = entity.GetComponent<Sprite2DComponent>();
                json c;
                c["type"] = "Sprite2D";
                c["displayName"] = "Sprite 2D";
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["tiling"] = { comp.tilingFactor.x, comp.tilingFactor.y };
                c["flipX"] = comp.flipX;
                c["flipY"] = comp.flipY;
                entry.components.push_back(c);
            }

            // Circle2D
            if (entity.HasComponent<Circle2DComponent>())
            {
                const auto &comp = entity.GetComponent<Circle2DComponent>();
                json c;
                c["type"] = "Circle2D";
                c["displayName"] = "Circle 2D";
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["thickness"] = comp.thickness;
                c["fade"] = comp.fade;
                entry.components.push_back(c);
            }

            // StaticMesh
            if (entity.HasComponent<StaticMeshComponent>())
            {
                json c;
                c["type"] = "StaticMesh";
                c["displayName"] = "Static Mesh";
                entry.components.push_back(c);
            }

            // SkeletalMesh
            if (entity.HasComponent<SkeletalMeshComponent>())
            {
                json c;
                c["type"] = "SkeletalMesh";
                c["displayName"] = "Skeletal Mesh";
                entry.components.push_back(c);
            }

            // Rigidbody
            if (entity.HasComponent<RigidbodyComponent>())
            {
                const auto &comp = entity.GetComponent<RigidbodyComponent>();
                json c;
                c["type"] = "Rigidbody";
                c["displayName"] = "Rigid Body";
                c["bodyType"] = static_cast<int>(comp.bodyType);
                c["mass"] = comp.mass;
                c["linearDamping"] = comp.linearDamping;
                c["angularDamping"] = comp.angularDamping;
                c["friction"] = comp.friction;
                c["restitution"] = comp.restitution;
                c["useGravity"] = comp.useGravity;
                entry.components.push_back(c);
            }

            // Rigidbody2D
            if (entity.HasComponent<Rigidbody2DComponent>())
            {
                const auto &comp = entity.GetComponent<Rigidbody2DComponent>();
                json c;
                c["type"] = "Rigidbody2D";
                c["displayName"] = "Rigid Body 2D";
                c["bodyType"] = static_cast<int>(comp.bodyType);
                c["gravityScale"] = comp.gravityScale;
                c["linearDamping"] = comp.linearDamping;
                c["angularDamping"] = comp.angularDamping;
                c["fixedRotation"] = comp.fixedRotation;
                c["isAwake"] = comp.isAwake;
                c["isEnabled"] = comp.isEnabled;
                entry.components.push_back(c);
            }

            // BoxCollider
            if (entity.HasComponent<BoxColliderComponent>())
            {
                const auto &comp = entity.GetComponent<BoxColliderComponent>();
                json c;
                c["type"] = "BoxCollider";
                c["displayName"] = "Box Collider";
                c["center"] = { comp.center.x, comp.center.y, comp.center.z };
                c["size"] = { comp.scale.x, comp.scale.y, comp.scale.z };
                entry.components.push_back(c);
            }

            // SphereCollider
            if (entity.HasComponent<SphereColliderComponent>())
            {
                const auto &comp = entity.GetComponent<SphereColliderComponent>();
                json c;
                c["type"] = "SphereCollider";
                c["displayName"] = "Sphere Collider";
                c["center"] = { comp.center.x, comp.center.y, comp.center.z };
                c["radius"] = comp.radius;
                entry.components.push_back(c);
            }

            // CapsuleCollider
            if (entity.HasComponent<CapsuleColliderComponent>())
            {
                const auto &comp = entity.GetComponent<CapsuleColliderComponent>();
                json c;
                c["type"] = "CapsuleCollider";
                c["displayName"] = "Capsule Collider";
                c["center"] = { comp.center.x, comp.center.y, comp.center.z };
                c["radius"] = comp.radius;
                c["height"] = comp.height;
                entry.components.push_back(c);
            }

            // MeshCollider
            if (entity.HasComponent<MeshColliderComponent>())
            {
                json c;
                c["type"] = "MeshCollider";
                c["displayName"] = "Mesh Collider";
                entry.components.push_back(c);
            }

            // BoxCollider2D
            if (entity.HasComponent<BoxCollider2DComponent>())
            {
                const auto &comp = entity.GetComponent<BoxCollider2DComponent>();
                json c;
                c["type"] = "BoxCollider2D";
                c["displayName"] = "Box Collider 2D";
                c["offset"] = { comp.offset.x, comp.offset.y };
                c["size"] = { comp.size.x, comp.size.y };
                c["density"] = comp.density;
                c["friction"] = comp.friction;
                c["restitution"] = comp.restitution;
                c["isSensor"] = comp.isSensor;
                entry.components.push_back(c);
            }

            // CircleCollider2D
            if (entity.HasComponent<CircleCollider2DComponent>())
            {
                const auto &comp = entity.GetComponent<CircleCollider2DComponent>();
                json c;
                c["type"] = "CircleCollider2D";
                c["displayName"] = "Circle Collider 2D";
                c["center"] = { comp.center.x, comp.center.y };
                c["radius"] = comp.radius;
                c["density"] = comp.density;
                c["friction"] = comp.friction;
                c["restitution"] = comp.restitution;
                c["isSensor"] = comp.isSensor;
                entry.components.push_back(c);
            }

            // AudioSource
            if (entity.HasComponent<AudioSourceComponent>())
            {
                const auto &comp = entity.GetComponent<AudioSourceComponent>();
                json c;
                c["type"] = "AudioSource";
                c["displayName"] = "Audio Source";
                c["volume"] = comp.volume;
                c["pitch"] = comp.pitch;
                c["pan"] = comp.pan;
                c["playOnStart"] = comp.playOnStart;
                c["loop"] = comp.loop;
                entry.components.push_back(c);
            }

            // Text
            if (entity.HasComponent<TextComponent>())
            {
                const auto &comp = entity.GetComponent<TextComponent>();
                json c;
                c["type"] = "Text";
                c["displayName"] = "Text";
                c["text"] = comp.text;
                c["color"] = { comp.color.r, comp.color.g, comp.color.b, comp.color.a };
                c["kerning"] = comp.kerning;
                c["lineSpacing"] = comp.lineSpacing;
                c["screenSpace"] = comp.screenSpace;
                entry.components.push_back(c);
            }

            // Script
            if (entity.HasComponent<ScriptComponent>())
            {
                const auto &comp = entity.GetComponent<ScriptComponent>();
                json c;
                c["type"] = "Script";
                c["displayName"] = "Script";
                c["className"] = comp.className;
                entry.components.push_back(c);
            }

            // WorldEnvironment
            if (entity.HasComponent<WorldEnvironment>())
            {
                const auto &comp = entity.GetComponent<WorldEnvironment>();
                json c;
                c["type"] = "WorldEnvironment";
                c["displayName"] = "World Environment";
                c["exposure"] = comp.exposure;
                c["gamma"] = comp.gamma;
                c["ambient"] = comp.ambient;
                c["fogDensity"] = comp.fogDensity;
                c["fogColor"] = { comp.fogColor.r, comp.fogColor.g, comp.fogColor.b, comp.fogColor.a };
                c["fogStart"] = comp.fogStart;
                c["fogEnd"] = comp.fogEnd;
                entry.components.push_back(c);
            }

            // CharacterController
            if (entity.HasComponent<CharacterControllerComponent>())
            {
                const auto &comp = entity.GetComponent<CharacterControllerComponent>();
                json c;
                c["type"] = "CharacterController";
                c["displayName"] = "Character Controller";
                c["radius"] = comp.radius;
                c["height"] = comp.height;
                c["maxStepHeight"] = comp.maxStepHeight;
                c["maxSlopeAngle"] = comp.maxSlopeAngle;
                c["mass"] = comp.mass;
                c["friction"] = comp.friction;
                entry.components.push_back(c);
            }

            entries[entry.id] = std::move(entry);
            allIdsInOrder.push_back(entry.id);
        }

        // Build parent-child tree
        for (uint64_t id : allIdsInOrder)
        {
            auto &entry = entries[id];
            if (entry.parent != 0 && entries.find(entry.parent) != entries.end())
            {
                entries[entry.parent].childIds.push_back(id);
            }
            else
            {
                rootIds.push_back(id);
            }
        }

        std::function<json(uint64_t)> buildNode = [&](uint64_t id) -> json
            {
                const auto &e = entries[id];
                json node;
                node["id"] = std::to_string(e.id);
                node["rawId"] = e.id;
                node["name"] = e.name;
                if (e.parent != 0)
                    node["parentId"] = std::to_string(e.parent);
                else
                    node["parentId"] = nullptr;
                node["isActive"] = e.isActive;
                node["components"] = e.components;

                json children = json::array();
                for (uint64_t childId : e.childIds)
                {
                    children.push_back(buildNode(childId));
                }
                node["children"] = children;
                return node;
            };

        json rootArray = json::array();
        for (uint64_t id : rootIds)
        {
            rootArray.push_back(buildNode(id));
        }

        return rootArray.dump();
    }
}
