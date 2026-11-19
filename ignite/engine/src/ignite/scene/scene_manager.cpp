// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "scene_manager.hpp"
#include "ignite/scripting/lua/lua_script_runtime.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "scene.hpp"
#include "prefab.hpp"
#include "ignite/core/application.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/project/project.hpp"

#include "entity.hpp"
#include "component.hpp"
#include "entity_command_manager.hpp"
#include "entity_destroy_command.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/serializer/entity_serializer.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/3d/physics_3d.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/uuid.hpp"

#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/ui/game_ui_system.hpp"
#include "ignite/net/network_manager.hpp"

#include "ignite/math/math.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace ignite
{
    static std::string StripTrailingNumberSuffix(const std::string &name)
    {
        if (name.size() < 4 || name.back() != ')')
            return name;

        const size_t openPos = name.rfind(" (");
        if (openPos == std::string::npos || openPos == 0)
            return name;

        const size_t digitsStart = openPos + 2;
        const size_t digitsEnd = name.size() - 1;
        if (digitsStart >= digitsEnd)
            return name;

        for (size_t i = digitsStart; i < digitsEnd; ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(name[i])))
                return name;
        }

        return name.substr(0, openPos);
    }

    static std::vector<UUID> *GetParentSiblingList(Scene *scene, UUID parentUUID)
    {
        if (!scene)
            return nullptr;

        if (parentUUID == UUID(0))
            return &scene->rootEntityOrder;

        Entity parent = SceneManager::GetEntity(scene, parentUUID);
        if (!parent.IsValid())
            return nullptr;

        return &parent.GetComponent<IDComponent>().children;
    }

    static void RemoveUUIDFromList(std::vector<UUID> &list, UUID target)
    {
        list.erase(std::remove(list.begin(), list.end(), target), list.end());
    }

    EntitySubtreeSnapshot CaptureEntitySubtreeSnapshot(Scene *scene, Entity rootEntity)
    {
        EntitySubtreeSnapshot snapshot;
        if (!scene || !scene->registry || !rootEntity.IsValid())
            return snapshot;

        const IDComponent &rootId = rootEntity.GetComponent<IDComponent>();
        snapshot.rootUUID = rootId.uuid;
        snapshot.parentUUID = rootId.parent;
        snapshot.rootName = rootId.name;
        snapshot.siblingIndex = SceneManager::GetSiblingIndex(scene, rootId.uuid);

        std::vector<Entity> orderedSubtree;
        std::unordered_set<UUID> visited;
        std::function<void(Entity)> collectSubtree = [&](Entity current)
        {
            if (!current.IsValid())
                return;

            const IDComponent &id = current.GetComponent<IDComponent>();
            if (visited.contains(id.uuid))
                return;

            visited.insert(id.uuid);
            orderedSubtree.push_back(current);
            snapshot.childOrders.emplace_back(id.uuid, id.children);

            for (UUID childUUID : id.children)
            {
                collectSubtree(SceneManager::GetEntity(scene, childUUID));
            }
        };

        collectSubtree(rootEntity);

        Serializer sr("");
        sr.BeginMap();
        sr.BeginSequence("Entities");
        for (Entity e : orderedSubtree)
        {
            EntitySerializer::SerializeEntity(sr, e);
        }
        sr.EndSequence();
        sr.EndMap();

        snapshot.serializedEntitiesJson = sr.GetRoot().dump();
        return snapshot;
    }

    Entity RestoreEntitySubtreeSnapshot(Scene *scene, const EntitySubtreeSnapshot &snapshot)
    {
        if (!scene || !scene->registry || !snapshot.IsValid())
            return {};

        JsonNode rootNode = Serializer::DeserializeFromString(snapshot.serializedEntitiesJson);
        JsonNode entitiesNode = rootNode["Entities"];
        if (!entitiesNode || !entitiesNode.IsSequence())
            return {};

        std::vector<UUID> restoredUUIDs;
        for (JsonNode entityNode : entitiesNode)
        {
            if (!entityNode["ID"])
                continue;

            const UUID uuid = UUID(entityNode["ID"].as<uint64_t>());
            if (!SceneManager::GetEntity(scene, uuid).IsValid())
            {
                EntitySerializer::DeserializeEntity(entityNode, scene, scene->GetProject());
            }
            restoredUUIDs.push_back(uuid);
        }

        // Restore exact child order for every entity in the subtree and keep non-root children out of rootEntityOrder
        for (const auto &[entityUUID, childOrder] : snapshot.childOrders)
        {
            if (entityUUID != snapshot.rootUUID)
            {
                RemoveUUIDFromList(scene->rootEntityOrder, entityUUID);
            }

            Entity e = SceneManager::GetEntity(scene, entityUUID);
            if (e.IsValid())
            {
                IDComponent &idComp = e.GetComponent<IDComponent>();
                idComp.children.clear();
                for (UUID childUUID : childOrder)
                {
                    if (SceneManager::GetEntity(scene, childUUID).IsValid())
                    {
                        idComp.children.push_back(childUUID);
                    }
                }
            }
        }

        // Place the restored root at its exact previous parent and sibling index
        UUID targetParent = snapshot.parentUUID;
        if (targetParent != UUID(0) && !SceneManager::GetEntity(scene, targetParent).IsValid())
        {
            targetParent = UUID(0);
        }

        SceneManager::MoveEntity(scene, snapshot.rootUUID, targetParent, snapshot.siblingIndex);

        for (UUID uuid : restoredUUIDs)
        {
            Entity e = SceneManager::GetEntity(scene, uuid);
            if (e.IsValid() && e.HasComponent<TransformComponent>())
            {
                e.GetComponent<TransformComponent>().dirty = true;
            }
        }

        scene->SetDirtyFlag(true);
        return SceneManager::GetEntity(scene, snapshot.rootUUID);
    }

    std::string SceneManager::GenerateUniqueEntityName(Scene *scene, const std::string &baseName)
    {
        const std::string fallback = baseName.empty() ? std::string("Entity") : baseName;
        if (!scene || !scene->registry)
            return fallback;

        std::unordered_set<std::string> existingNames;
        auto view = scene->registry->view<IDComponent>();
        for (entt::entity e : view)
        {
            existingNames.insert(view.get<IDComponent>(e).name);
        }

        if (!existingNames.contains(fallback))
            return fallback;

        std::string stem = StripTrailingNumberSuffix(fallback);
        if (stem.empty())
            stem = "Entity";

        uint32_t counter = 1;
        while (true)
        {
            std::string candidate = fmt::format("{} ({})", stem, counter);
            if (!existingNames.contains(candidate))
                return candidate;
            ++counter;
        }
    }

    Entity SceneManager::CreateEntity(Scene *scene, const std::string &name, EntityType type, UUID uuid)
    {
        scene->SetDirtyFlag(true);
        Entity entity = Entity { scene->registry->create(), scene };
        entity.AddComponent<IDComponent>(name, type, uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RenderingComponent>();
        scene->entities[uuid] = entity;
        if (std::find(scene->rootEntityOrder.begin(), scene->rootEntityOrder.end(), uuid) == scene->rootEntityOrder.end())
        {
            scene->rootEntityOrder.push_back(uuid);
        }
        return entity;
    }

    Entity SceneManager::CreateFolder(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        const std::string base = name.empty() ? std::string("Folder") : name;
        const std::string uniqueName = GenerateUniqueEntityName(scene, base);
        Entity folder = CreateEntity(scene, uniqueName, EntityType_Folder, uuid);

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && folder.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, folder, "Create Folder"));
        }
        return folder;
    }

    Entity SceneManager::CreateSprite(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        Entity createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
        createdEntity.AddComponent<Sprite2DComponent>();

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + name));
        }

        return createdEntity;
    }

    Entity SceneManager::CreateCircle(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        Entity createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
        createdEntity.AddComponent<Circle2DComponent>();

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + name));
        }

        return createdEntity;
    }

    Entity SceneManager::CreatePointLight2D(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        Entity createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
        createdEntity.AddComponent<PointLight2DComponent>();

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + name));
        }

        return createdEntity;
    }

    Entity SceneManager::CreateMesh(Scene *scene, const std::string &name, UUID uuid)
    {
        scene->SetDirtyFlag(true);

        Entity entity = Entity { scene->registry->create(), scene };
        entity.AddComponent<IDComponent>(name, EntityType_Node, uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RenderingComponent>();
        scene->entities[uuid] = entity;
        if (std::find(scene->rootEntityOrder.begin(), scene->rootEntityOrder.end(), uuid) == scene->rootEntityOrder.end())
        {
            scene->rootEntityOrder.push_back(uuid);
        }

        return entity;
    }

    Entity SceneManager::CreatePrimitive(Scene *scene, EMeshType type, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        const std::string entityName = name.empty() ? std::string(Renderer::GetDefaultMeshName(type)) : name;
        const AssetHandle meshHandle = Renderer::GetDefaultMeshHandle(type);

        Entity createdEntity = CreateEntity(scene, entityName, EntityType_Node | EntityType_Mesh, uuid);
        auto &smc = createdEntity.AddComponent<StaticMeshComponent>();
        smc.handle = meshHandle;
        if (auto mesh = Renderer::GetDefaultMesh(type))
        {
            smc.worldAABB = mesh->localAABB;
        }

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + entityName));
        }

        return createdEntity;
    }

    Entity SceneManager::CreateCamera(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        Entity createdEntity = CreateEntity(scene, name, EntityType_Camera, uuid);
        createdEntity.AddComponent<CameraComponent>();

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + name));
        }

        return createdEntity;
    }

    Entity SceneManager::CreateWorldEnvironment(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        Entity createdEntity = CreateEntity(scene, name, EntityType_WorldEnvironment, uuid);
        createdEntity.AddComponent<WorldEnvironment>();

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + name));
        }

        return createdEntity;
    }

    Entity SceneManager::CreateEmptyEntity(Scene *scene, const std::string &name, UUID uuid)
    {
        if (!scene)
            return Entity{};

        Entity createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);

        if (!scene->IsRunning() && !CommandManager::IsExecutingUndoRedo() && createdEntity.IsValid())
        {
            CommandManager::AddCommand(CreateScope<EntityCreateCommand>(scene, createdEntity, "Create " + name));
        }

        return createdEntity;
    }

    void SceneManager::RenameEntity(Scene *scene, Entity entity, const std::string &newName)
    {
        scene->SetDirtyFlag(true);

        if (newName.empty())
            return;

        IDComponent &idComp = entity.GetComponent<IDComponent>();
        idComp.name = newName;
    }

    void SceneManager::DestroyEntity(Scene *scene, Entity entity)
    {
        if (!scene || !scene->registry || !scene->registry->valid(entity))
            return;

        scene->SetDirtyFlag(true);

        if (scene->GetLuaScriptRuntime() && entity.HasComponent<LuaScriptComponent>())
            scene->GetLuaScriptRuntime()->DestroyInstance(entity.GetUUID());

        if (entity.HasComponent<PhysicsJointComponent>())
        {
            auto &jointComp = entity.GetComponent<PhysicsJointComponent>();
            if (auto jointActor = jointComp.runtimeJoint.lock())
            {
                if (scene->GetPhysics3D())
                {
                    scene->GetPhysics3D()->DestroyJoint(jointActor);
                }
                else
                {
                    jointActor->DestroyJoint();
                }
            }
            jointComp.runtimeJoint.reset();
        }

        // Destroy any joints on other entities that are connected to this entity
        const UUID entityUuid = entity.GetUUID();
        for (entt::entity e : scene->registry->view<PhysicsJointComponent>())
        {
            auto &otherJoint = scene->registry->get<PhysicsJointComponent>(e);
            if (otherJoint.connectedEntity == entityUuid)
            {
                if (auto jointActor = otherJoint.runtimeJoint.lock())
                {
                    if (scene->GetPhysics3D())
                    {
                        scene->GetPhysics3D()->DestroyJoint(jointActor);
                    }
                    else
                    {
                        jointActor->DestroyJoint();
                    }
                }
                otherJoint.runtimeJoint.reset();
            }
        }

        if (entity.HasComponent<RigidbodyComponent>())
        {
            auto &rb = entity.GetComponent<RigidbodyComponent>();
            if (auto dynActor = rb.dynamicActor.lock()) dynActor->DestroyBody();
            if (auto statActor = rb.staticActor.lock()) statActor->DestroyBody();
        }
        scene->DestroyPhysics2DBody(entity);

        IDComponent idComp = entity.GetComponent<IDComponent>();

        // recursively destroy children
        for (UUID childId : idComp.children)
        {
            entity.GetComponent<IDComponent>().RemoveChild(childId);
            DestroyEntity(scene, GetEntity(scene, childId));
        }

        if (entity.HasComponent<NetworkIdentityComponent>())
        {
            const auto &netComp = entity.GetComponent<NetworkIdentityComponent>();
            scene->UnregisterNetEntity(netComp.netId);
        }

        RemoveUUIDFromList(scene->rootEntityOrder, idComp.uuid);
        scene->registry->destroy(entity);
        scene->entities.erase(idComp.uuid);

        // remove from parent
        if (idComp.parent != UUID(0))
        {
            Entity parent = SceneManager::GetEntity(scene, idComp.parent);
            if (parent.IsValid())
            {
                parent.GetComponent<IDComponent>().RemoveChild(idComp.uuid);
            }
        }
    }

    void SceneManager::DestroyEntity(Scene *scene, UUID uuid)
    {
        DestroyEntity(scene, GetEntity(scene, uuid));
    }

    Entity SceneManager::DuplicateEntity(Scene *scene, Entity entity, bool addToParent, bool recordUndo)
    {
        if (!scene || !entity.IsValid())
            return {};

        scene->SetDirtyFlag(true);

        // Copy IDComponent fields BEFORE creating newEntity because registry insertion can reallocate pools.
        const IDComponent origIdComp = entity.GetComponent<IDComponent>();
        const std::string newName = addToParent ? GenerateUniqueEntityName(scene, origIdComp.name) : origIdComp.name;

        Entity newEntity = SceneManager::CreateEntity(scene, newName, origIdComp.type);

        // copy current entity's components to new entity
        SceneManager::CopyComponentIfExists(AllComponents{}, newEntity, entity);

        if (newEntity.HasComponent<NetworkIdentityComponent>())
        {
            auto &netComp = newEntity.GetComponent<NetworkIdentityComponent>();
            if (netComp.netId != 0)
            {
                const uint64_t copiedNetId = netComp.netId;
                scene->UnregisterNetEntity(copiedNetId);
                scene->RegisterNetEntity(copiedNetId, entity);
            }
            netComp.netId = 0; // Avoid duplicate netId in the same scene
            netComp.ownerClient = 0;
        }

        const UUID newUUID = newEntity.GetUUID();
        {
            IDComponent &newEntityIDComp = newEntity.GetComponent<IDComponent>();
            newEntityIDComp.parent = UUID(0);
            newEntityIDComp.children.clear();
        }

        // Recursively duplicate children without emitting separate undo commands per child
        for (UUID cid : origIdComp.children)
        {
            Entity srcChild = GetEntity(scene, cid);
            if (!srcChild.IsValid())
                continue;

            Entity newChildEntity = DuplicateEntity(scene, srcChild, false, false);
            if (newChildEntity.IsValid())
            {
                MoveEntity(scene, newChildEntity.GetUUID(), newUUID);
            }
        }

        if (addToParent)
        {
            // Place duplicated entity right below the original entity within the same parent (or root)
            ReorderRelativeTo(scene, newUUID, origIdComp.uuid, HierarchyDropPosition::Below);
        }

        // Do not copy Particle System 
        if (newEntity.HasComponent<ParticleSystemComponent>())
        {
            auto &particleSystem = newEntity.GetComponent<ParticleSystemComponent>();
            particleSystem.system = nullptr;
        }

        if (newEntity.HasComponent<PhysicsJointComponent>())
        {
            newEntity.GetComponent<PhysicsJointComponent>().runtimeJoint.reset();
        }

        if (newEntity.HasComponent<ScriptComponent>())
        {
            newEntity.GetComponent<ScriptComponent>().runtimeScriptInstance = nullptr;
        }

        if (scene->IsRunning())
        {
            // IMPORTANT!: Need to reset copied body from source
            //             Should have a Brand new body
            if (newEntity.HasComponent<RigidbodyComponent>())
            {
                auto &rb = newEntity.GetComponent<RigidbodyComponent>();
                rb.dynamicActor.reset();
                rb.staticActor.reset();

                if (scene->GetPhysics3D())
                {
                    auto &tr = newEntity.GetTransform();
                    uint64_t userData = static_cast<uint64_t>(newEntity.GetUUID());

                    physics::RigidBodyDesc rbDesc;
                    rbDesc.bodyType = rb.bodyType;
                    rbDesc.motionQuality = rb.motionQuality;
                    rbDesc.useGravity = rb.useGravity;
                    rbDesc.mass = rb.mass;
                    rbDesc.friction = rb.friction;
                    rbDesc.restitution = rb.restitution;

                    physics::PhysicsTransformData transformData;
                    transformData.position = tr.world.translation;
                    transformData.rotation = tr.world.rotation;

                    if (rb.bodyType == physics::BodyType::Dynamic || rb.bodyType == physics::BodyType::Kinematic)
                    {
                        rb.dynamicActor = scene->GetPhysics3D()->CreateDynamicBody(rbDesc, transformData, userData);
                    }
                    else
                    {
                        rb.staticActor = scene->GetPhysics3D()->CreateStaticBody(rbDesc, transformData, userData);
                    }
                }
            }

            if (newEntity.HasComponent<CharacterControllerComponent>())
            {
                auto &cc = newEntity.GetComponent<CharacterControllerComponent>();
                cc.character.reset();

                if (scene->GetPhysics3D() && scene->IsRunning())
                {
                    auto &tr = newEntity.GetTransform();
                    uint64_t userData = static_cast<uint64_t>(newEntity.GetUUID());

                    const float maxScale = glm::compMax(glm::abs(tr.world.scale));
                    const float radius = cc.radius * maxScale;
                    const float halfHeight = glm::max(cc.height * 0.5f - cc.radius, 0.0f) * maxScale;

                    physics::CharacterControllerDesc ccDesc;
                    ccDesc.center = cc.center * tr.world.scale;
                    ccDesc.radius = radius;
                    ccDesc.halfHeight = halfHeight;
                    ccDesc.mass = cc.mass;
                    ccDesc.friction = cc.friction;
                    ccDesc.gravityFactor = cc.gravityFactor;
                    ccDesc.maxStepHeight = cc.maxStepHeight;
                    ccDesc.maxSlopeAngle = cc.maxSlopeAngle;
                    ccDesc.up = cc.up;

                    auto charActor = scene->GetPhysics3D()->CreateCharacterController(ccDesc, userData);
                    if (charActor)
                    {
                        charActor->SetPosition(tr.world.translation);
                        charActor->SetRotation(tr.world.rotation);
                        charActor->SetLinearVelocity(cc.linearVelocity);
                    }
                    cc.character = charActor;
                }
            }

            if (newEntity.HasComponent<PhysicsJointComponent>() && newEntity.HasComponent<RigidbodyComponent>() && scene->GetPhysics3D())
            {
                auto &jointComp = newEntity.GetComponent<PhysicsJointComponent>();
                auto &rbA = newEntity.GetComponent<RigidbodyComponent>();
                auto &trA = newEntity.GetTransform();

                auto dynamicBodyA = rbA.dynamicActor.lock();
                auto staticBodyA = rbA.staticActor.lock();
                if (dynamicBodyA || staticBodyA)
                {
                    physics::TransformDesc transformA;
                    transformA.position = trA.world.translation;
                    transformA.rotation = trA.world.rotation;
                    transformA.scale = trA.world.scale;

                    Ref<physics::PhysicsDynamicActor> dynamicBodyB = nullptr;
                    Ref<physics::PhysicsStaticActor> staticBodyB = nullptr;
                    physics::TransformDesc transformB;

                    if (jointComp.connectedEntity != UUID(0) && jointComp.connectedEntity != newEntity.GetUUID())
                    {
                        Entity connected = GetEntity(scene, jointComp.connectedEntity);
                        if (connected.IsValid())
                        {
                            auto &trB = connected.GetTransform();
                            transformB.position = trB.world.translation;
                            transformB.rotation = trB.world.rotation;
                            transformB.scale = trB.world.scale;

                            if (connected.HasComponent<RigidbodyComponent>())
                            {
                                auto &rbB = connected.GetComponent<RigidbodyComponent>();
                                dynamicBodyB = rbB.dynamicActor.lock();
                                staticBodyB = rbB.staticActor.lock();
                            }
                        }
                    }

                    jointComp.runtimeJoint = scene->GetPhysics3D()->CreateJoint(
                        jointComp.desc,
                        static_cast<uint64_t>(newEntity.GetUUID()),
                        dynamicBodyA,
                        staticBodyA,
                        transformA,
                        dynamicBodyB,
                        staticBodyB,
                        transformB
                    );
                }
            }

            scene->CreatePhysics2DBody(newEntity);

            if (scene->IsPlaying())
            {
                if (newEntity.HasComponent<ScriptComponent>())
                {
                    auto &sc = newEntity.GetComponent<ScriptComponent>();
                    if (!sc.className.empty() && sc.className != "EMPTY" && ScriptEngine::GetInstance())
                    {
                        sc.runtimeScriptInstance = ScriptEngine::GetInstance()->OnCreateEntityInstance(newEntity.GetUUID(), sc.className);
                    }
                }

                if (newEntity.HasComponent<LuaScriptComponent>())
                {
                    if (lua::ScriptRuntime *luaRuntime = scene->GetLuaScriptRuntime())
                    {
                        const auto &luaScript = newEntity.GetComponent<LuaScriptComponent>();
                        std::filesystem::path path = luaScript.scriptPath;
                        if (luaScript.scriptHandle != AssetHandle(0) && scene->GetAssetManager())
                        {
                            const auto &meta = scene->GetAssetManager()->GetMetaData(luaScript.scriptHandle);
                            if (!meta.filepath.empty())
                                path = meta.filepath;
                        }
                        luaRuntime->CreateInstance(newEntity.GetUUID(), path, luaScript.scriptHandle);
                    }
                }
            }
        }

        if (!scene->IsRunning() && recordUndo && !CommandManager::IsExecutingUndoRedo() && newEntity.IsValid())
        {
            CommandManager::AddCommand(
                CreateScope<EntityCreateCommand>(scene, newEntity, "Duplicate " + origIdComp.name));
        }

        return newEntity;
    }

    Entity SceneManager::GetEntity(Scene *scene, UUID uuid)
    {
        if (scene && scene->entities.contains(uuid))
        {
            entt::entity handle = scene->entities.at(uuid);
            if (scene->registry && scene->registry->valid(handle))
                return Entity{ handle, scene };
        }

        return Entity{};
    }

    Entity SceneManager::GetEntity(Scene *scene, const std::string &name)
    {
        auto view = scene->registry->view<IDComponent>();
        for (entt::entity e : view)
        {
            const IDComponent &id = view.get<IDComponent>(e);
            if (id.name == name)
            {
                return Entity{ e, scene };
            }
        }
        return Entity{};
    }

    void SceneManager::SyncHierarchyOrder(Scene *scene)
    {
        if (!scene || !scene->registry)
            return;

        std::unordered_set<UUID> seenRoots;
        std::vector<UUID> validRoots;
        validRoots.reserve(scene->rootEntityOrder.size());

        for (UUID uuid : scene->rootEntityOrder)
        {
            if (seenRoots.contains(uuid))
                continue;

            Entity e = GetEntity(scene, uuid);
            if (e.IsValid() && e.GetComponent<IDComponent>().parent == UUID(0))
            {
                seenRoots.insert(uuid);
                validRoots.push_back(uuid);
            }
        }

        // Append any missing root entities not yet listed in rootEntityOrder
        auto view = scene->registry->view<IDComponent>();
        for (entt::entity handle : view)
        {
            const IDComponent &idComp = view.get<IDComponent>(handle);
            if (idComp.parent == UUID(0) && !seenRoots.contains(idComp.uuid))
            {
                seenRoots.insert(idComp.uuid);
                validRoots.push_back(idComp.uuid);
            }
        }
        scene->rootEntityOrder = std::move(validRoots);

        // Clean stale or duplicate entries inside every entity's children vector
        for (entt::entity handle : view)
        {
            IDComponent &idComp = view.get<IDComponent>(handle);
            if (idComp.children.empty())
                continue;

            std::unordered_set<UUID> seenChildren;
            std::vector<UUID> validChildren;
            validChildren.reserve(idComp.children.size());

            for (UUID childUUID : idComp.children)
            {
                if (seenChildren.contains(childUUID))
                    continue;

                Entity child = GetEntity(scene, childUUID);
                if (child.IsValid() && child.GetComponent<IDComponent>().parent == idComp.uuid)
                {
                    seenChildren.insert(childUUID);
                    validChildren.push_back(childUUID);
                }
            }
            idComp.children = std::move(validChildren);
        }
    }

    size_t SceneManager::GetSiblingIndex(Scene *scene, UUID entityUUID)
    {
        Entity entity = GetEntity(scene, entityUUID);
        if (!entity.IsValid())
            return 0;

        const UUID parentUUID = entity.GetComponent<IDComponent>().parent;
        const std::vector<UUID> *list = GetParentSiblingList(scene, parentUUID);
        if (!list)
            return 0;

        for (size_t i = 0; i < list->size(); ++i)
        {
            if ((*list)[i] == entityUUID)
                return i;
        }
        return list->size();
    }

    bool SceneManager::MoveEntity(Scene *scene, UUID entityUUID, UUID newParentUUID, size_t insertIndex)
    {
        if (!scene || !scene->registry)
            return false;

        Entity src = GetEntity(scene, entityUUID);
        if (!src.IsValid())
            return false;

        if (newParentUUID != UUID(0))
        {
            Entity dest = GetEntity(scene, newParentUUID);
            if (!dest.IsValid())
                return false;

            if (newParentUUID == entityUUID || IsParent(scene, newParentUUID, entityUUID))
                return false;
        }

        IDComponent &srcID = src.GetComponent<IDComponent>();
        const UUID oldParentUUID = srcID.parent;

        if (std::vector<UUID> *oldList = GetParentSiblingList(scene, oldParentUUID))
        {
            RemoveUUIDFromList(*oldList, entityUUID);
        }

        srcID.parent = newParentUUID;

        std::vector<UUID> *newList = GetParentSiblingList(scene, newParentUUID);
        if (!newList)
        {
            srcID.parent = UUID(0);
            newList = &scene->rootEntityOrder;
        }

        RemoveUUIDFromList(*newList, entityUUID);
        const size_t clampedIndex = (std::min)(insertIndex, newList->size());
        newList->insert(newList->begin() + static_cast<ptrdiff_t>(clampedIndex), entityUUID);

        if (src.HasComponent<TransformComponent>())
        {
            src.GetComponent<TransformComponent>().dirty = true;
        }

        scene->SetDirtyFlag(true);
        return true;
    }

    bool SceneManager::ReorderRelativeTo(Scene *scene, UUID sourceUUID, UUID targetUUID, HierarchyDropPosition position)
    {
        if (!scene || !scene->registry || sourceUUID == targetUUID)
            return false;

        Entity src = GetEntity(scene, sourceUUID);
        Entity target = GetEntity(scene, targetUUID);
        if (!src.IsValid() || !target.IsValid())
            return false;

        if (position == HierarchyDropPosition::Inside)
        {
            return MoveEntity(scene, sourceUUID, targetUUID, (std::numeric_limits<size_t>::max)());
        }

        const UUID targetParentUUID = target.GetComponent<IDComponent>().parent;
        if (targetParentUUID != UUID(0) && (targetParentUUID == sourceUUID || IsParent(scene, targetParentUUID, sourceUUID)))
        {
            return false;
        }

        const std::vector<UUID> *destList = GetParentSiblingList(scene, targetParentUUID);
        if (!destList)
            return false;

        size_t targetIdx = destList->size();
        for (size_t i = 0; i < destList->size(); ++i)
        {
            if ((*destList)[i] == targetUUID)
            {
                targetIdx = i;
                break;
            }
        }
        if (targetIdx >= destList->size())
            return false;

        const UUID oldParentUUID = src.GetComponent<IDComponent>().parent;
        size_t finalIdx = (position == HierarchyDropPosition::Above) ? targetIdx : (targetIdx + 1);
        if (oldParentUUID == targetParentUUID)
        {
            const size_t oldIdx = GetSiblingIndex(scene, sourceUUID);
            if (oldIdx < finalIdx)
            {
                --finalIdx;
            }
        }

        return MoveEntity(scene, sourceUUID, targetParentUUID, finalIdx);
    }

    bool SceneManager::AddChild(Scene *scene, Entity destination, Entity source)
    {
        if (!destination.IsValid() || !source.IsValid() || source == destination)
            return false;

        return MoveEntity(scene, source.GetUUID(), destination.GetUUID(), (std::numeric_limits<size_t>::max)());
    }

    bool SceneManager::ChildExists(Scene *scene, Entity destination, Entity source)
    {
        IDComponent &destIDComp = destination.GetComponent<IDComponent>();
        IDComponent &sourceIDComp = source.GetComponent<IDComponent>();

        // source parent is the destination
        if (sourceIDComp.parent == destIDComp.uuid)
        {
            return true;
        }

        // recursively search
        Entity nextParent = GetEntity(scene, sourceIDComp.parent);
        if (ChildExists(scene, nextParent, source)) // find until it is not source's parent
            return true;

        return false;
    }

    bool SceneManager::IsParent(Scene *scene, UUID target, UUID source)
    {
        Entity destParent = GetEntity(scene, target);
        if (!destParent.IsValid())
            return false;

        const IDComponent &destIDComp = destParent.GetComponent<IDComponent>();

        if (target == source)
            return true;

        if (destIDComp.parent && IsParent(scene, destIDComp.parent, source))
            return true;

        return false;
    }

    Entity SceneManager::FindChild(Scene *scene, Entity parent, UUID uuid)
    {
        UUID parentUUID = parent.GetComponent<IDComponent>().uuid;
        if (IsParent(scene, parentUUID, uuid))
            return GetEntity(scene, uuid);

        return {entt::null, nullptr};
    }

    Ref<Scene> SceneManager::Copy(Ref<Scene> &other)
    {
        if (!other)
        {
            LOG_WARN("[SceneManager] Copy called with null scene");
            return nullptr;
        }

        // create new scene with other's name
        Ref<Scene> newScene = CreateRef<Scene>(other->GetProject());

        // create source and destination registry
        auto srcRegistry = other->registry;
        auto destRegistry = newScene->registry;

        EntityMap entityMap;

        // create entities for new new scene
        auto view = srcRegistry->view<IDComponent>();
        for (auto e : view)
        {
            // get src entity component
            Entity srcEntity = { e, other.get() };
            IDComponent &srcIdComp = srcEntity.GetComponent<IDComponent>();

            // store src entity component to new entity (destination entity)
            Entity newEntity = SceneManager::CreateEntity(newScene.get(), srcIdComp.name, srcIdComp.type, srcIdComp.uuid);
            IDComponent &newEntityIdComp = newEntity.GetComponent<IDComponent>();
            newEntityIdComp.parent = srcIdComp.parent;
            newEntityIdComp.children = srcIdComp.children;
            newEntityIdComp.type = srcIdComp.type;

            entityMap[srcIdComp.uuid] = newEntity;
        }

        SceneManager::CopyComponent(AllComponents{}, destRegistry, srcRegistry, entityMap);

        // Re-register network entities in the copied scene
        auto netView = destRegistry->view<NetworkIdentityComponent>();
        for (entt::entity e : netView)
        {
            auto &netComp = netView.get<NetworkIdentityComponent>(e);
            if (netComp.netId != 0)
            {
                newScene->RegisterNetEntity(netComp.netId, Entity{ e, newScene.get() });
            }
        }

        // copy scene extra data
        newScene->handle = other->handle;
        newScene->m_LoadedAssets = other->m_LoadedAssets;
        newScene->rootEntityOrder = other->rootEntityOrder;
        SyncHierarchyOrder(newScene.get());

        // Do not copy entities (it will be created when creating entity)
        // newScene->entities = other->entities;

        // Do not copy registered comps
        // newScene->registeredComps = other->registeredComps;

        /*auto mrView = destRegistry->view<MeshRenderer>();
        for (entt::entity e : mrView)
        {
            MeshRenderer &mr = mrView.get<MeshRenderer>(e);
            bool isSkinnedMesh = true;
            mr.Create(isSkinnedMesh);
            mr.mesh->CreateBuffers();
            mr.mesh->WriteVertexBuffer(static_cast<uint32_t>(e));
        }*/

        return newScene;
    }

    bool SceneManager::s_TransitionPending = false;
    AssetHandle SceneManager::s_PendingSceneHandle = AssetHandle(0);
    bool SceneManager::s_StopPending = false;
    std::function<void()> SceneManager::s_StopCallback = nullptr;

    void SceneManager::RequestStop()
    {
        s_StopPending = true;
    }

    void SceneManager::SetStopCallback(std::function<void()> callback)
    {
        s_StopCallback = std::move(callback);
    }

    void SceneManager::Transition(AssetHandle nextSceneHandle)
    {
        if (s_TransitionPending)
        {
            LOG_WARN("[Scene Manager] A transition is already pending! Ignoring request.");
            return;
        }

        if (nextSceneHandle == AssetHandle(0))
        {
            LOG_ERROR("[Scene Manager] Cannot transition to a null/invalid scene handle!");
            return;
        }

        auto* assetManager = AssetManager::GetInstance();
        if (!assetManager)
        {
            LOG_ERROR("[Scene Manager] No active AssetManager found during transition request.");
            return;
        }

        if (!assetManager->IsAssetHandleValid(nextSceneHandle))
        {
            LOG_ERROR("[Scene Manager] Scene handle {} is invalid!", static_cast<uint64_t>(nextSceneHandle));
            return;
        }

        Application::SubmitToMainThread([nextSceneHandle, assetManager]()
        {
            // Load incoming scene async so we can collect its referenced assets
            Ref<Scene> nextScene = assetManager->GetAsset<Scene>(nextSceneHandle);
            s_TransitionPending = true;
            s_PendingSceneHandle = nextSceneHandle;
        });
    }

    void SceneManager::ExecutePendingTransition()
    {
        if (s_StopPending)
        {
            s_StopPending = false;
            s_TransitionPending = false;
            s_PendingSceneHandle = AssetHandle(0);

            if (s_StopCallback)
            {
                s_StopCallback();
            }
            else if (Application *app = Application::GetInstance())
            {
                app->Shutdown();
            }
            return;
        }

        if (!s_TransitionPending)
            return;

        auto* assetManager = AssetManager::GetInstance();
        if (!assetManager)
            return;

        AssetHandle nextSceneHandle = s_PendingSceneHandle;
        Ref<Scene> loadedScene = assetManager->GetAsset<Scene>(nextSceneHandle);
        if (!loadedScene)
        {
            // Asset is still loading on worker thread, defer transition execution
            return;
        }

        loadedScene->PreloadReferencedAssets();
        if (!loadedScene->IsReadyToPlay())
        {
            // Related assets are not ready yet, defer transition execution
            return;
        }

        s_TransitionPending = false;
        s_PendingSceneHandle = AssetHandle(0);

        Ref<Project> project = assetManager->LockActiveProject();
        if (!project)
            return;

        Ref<Scene> currentScene = project->LockActiveScene();
        ESceneState previousState = ESceneState::Stop;
        if (currentScene)
        {
            previousState = currentScene->GetState();
            currentScene->OnStop();
        }
        if (!loadedScene)
        {
            LOG_ERROR("[Scene Manager] Failed to load transition target scene!");
            return;
        }

        Ref<Scene> transitionScene = SceneManager::Copy(loadedScene);
        if (!transitionScene)
        {
            LOG_ERROR("[Scene Manager] Failed to copy transition scene!");
            return;
        }

        project->SetActiveScene(transitionScene);
        GameUISystem::SetSceneContext(transitionScene.get());
        if (auto *nm = NetworkManager::GetInstance())
        {
            nm->SetActiveScene(transitionScene.get());
        }
        transitionScene->OnStart(previousState);

        assetManager->UnloadUnusedAssets();

        LOG_INFO("[Scene Manager] Successfully transitioned to scene {}", project->GetAssetDisplayName(nextSceneHandle));
    }

    Entity SceneManager::CloneEntityTree(Scene *destScene, Scene *srcScene, Entity srcRoot)
    {
        if (!destScene || !srcScene || !srcRoot.IsValid())
            return { entt::null, nullptr };

        const IDComponent &srcID = srcRoot.GetComponent<IDComponent>();
        const std::vector<UUID> srcChildren = srcID.children;

        Entity destEntity = SceneManager::CreateEntity(destScene, srcID.name, srcID.type);

        SceneManager::CopyComponentIfExists(AllComponents{}, destEntity, srcRoot);

        const UUID destUUID = destEntity.GetUUID();
        {
            IDComponent &destID = destEntity.GetComponent<IDComponent>();
            destID.parent = UUID(0);
            destID.children.clear();
        }

        for (UUID childUUID : srcChildren)
        {
            Entity srcChild = SceneManager::GetEntity(srcScene, childUUID);
            if (srcChild.IsValid())
            {
                Entity destChild = CloneEntityTree(destScene, srcScene, srcChild);
                if (destChild.IsValid())
                {
                    MoveEntity(destScene, destChild.GetUUID(), destUUID);
                }
            }
        }

        return destEntity;
    }

    Entity SceneManager::InstantiatePrefab(Scene *scene, const Ref<Prefab> &prefab, Entity parent)
    {
        if (!scene || !prefab || !prefab->GetPrefabScene())
            return { entt::null, nullptr };

        UUID rootUUID = prefab->GetRootEntityUUID();
        Entity prefabRoot = SceneManager::GetEntity(prefab->GetPrefabScene().get(), rootUUID);
        if (!prefabRoot.IsValid())
        {
            if (!prefab->GetPrefabScene()->entities.empty())
            {
                prefabRoot = { prefab->GetPrefabScene()->entities.begin()->second, prefab->GetPrefabScene().get() };
            }
        }

        if (!prefabRoot.IsValid())
            return { entt::null, nullptr };

        Entity instanceRoot = CloneEntityTree(scene, prefab->GetPrefabScene().get(), prefabRoot);
        if (instanceRoot.IsValid())
        {
            if (!instanceRoot.HasComponent<PrefabComponent>())
            {
                instanceRoot.AddComponent<PrefabComponent>(prefab->handle, prefabRoot.GetUUID());
            }
            else
            {
                auto &comp = instanceRoot.GetComponent<PrefabComponent>();
                comp.prefabHandle = prefab->handle;
                comp.prefabEntityUUID = prefabRoot.GetUUID();
            }

            if (parent.IsValid())
            {
                AddChild(scene, parent, instanceRoot);
            }
        }

        scene->SetDirtyFlag(true);
        return instanceRoot;
    }

    void SceneManager::SyncAllPrefabInstances(Scene *scene, AssetHandle prefabHandle)
    {
        if (!scene || prefabHandle == AssetHandle(0))
            return;

        AssetWorker::SubmitJob([scene, prefabHandle]()
        {
            auto assetManager = AssetManager::GetInstance();
            if (!assetManager)
                return;

            Ref<Prefab> prefab = assetManager->GetAssetImmediate<Prefab>(prefabHandle);
            if (!prefab || !prefab->GetPrefabScene())
                return;

            Application::SubmitToMainThread([scene, prefabHandle, prefab]()
            {
                std::vector<Entity> instancesToSync;
                if (scene->registry)
                {
                    auto view = scene->registry->view<PrefabComponent>();
                    for (entt::entity e : view)
                    {
                        const auto &comp = view.get<PrefabComponent>(e);
                        if (comp.prefabHandle == prefabHandle)
                        {
                            Entity entity{ e, scene };
                            if (entity.GetParentUUID() == UUID(0))
                            {
                                instancesToSync.push_back(entity);
                            }
                            else
                            {
                                Entity parent = GetEntity(scene, entity.GetParentUUID());
                                if (!parent.IsValid() || !parent.HasComponent<PrefabComponent>() || parent.GetComponent<PrefabComponent>().prefabHandle != prefabHandle)
                                {
                                    instancesToSync.push_back(entity);
                                }
                            }
                        }
                    }
                }

                for (Entity instanceRoot : instancesToSync)
                {
                    if (!instanceRoot.IsValid())
                        continue;

                    const auto &idComp = instanceRoot.GetComponent<IDComponent>();
                    UUID parentUUID = idComp.parent;
                    std::string entityName = idComp.name;
                    TransformComponent oldTransform = instanceRoot.GetComponent<TransformComponent>();

                    Entity newRoot = InstantiatePrefab(scene, prefab);
                    if (newRoot.IsValid())
                    {
                        newRoot.GetComponent<TransformComponent>() = oldTransform;
                        newRoot.GetComponent<IDComponent>().name = entityName;

                        if (parentUUID != UUID(0))
                        {
                            Entity parent = GetEntity(scene, parentUUID);
                            if (parent.IsValid())
                            {
                                AddChild(scene, parent, newRoot);
                            }
                        }

                        DestroyEntity(scene, instanceRoot);
                    }
                }
            });
        });
    }

    void SceneManager::ApplyPrefabChanges(Entity entity, Scene *scene, Project *project)
    {
        if (!entity.IsValid() || !scene || !entity.HasComponent<PrefabComponent>())
            return;

        AssetHandle prefabHandle = entity.GetComponent<PrefabComponent>().prefabHandle;
        if (prefabHandle == AssetHandle(0))
            return;

        AssetWorker::SubmitJob([e = entity, prefabHandle, scene, project]()
        {
            auto assetManager = AssetManager::GetInstance();
            if (!assetManager)
                return;

            Ref<Prefab> prefab = assetManager->GetAssetImmediate<Prefab>(prefabHandle);
            if (!prefab)
                return;

            Application::SubmitToMainThread([e, prefabHandle, scene, project, prefab]()
            {
                if (prefab->UpdateFromEntity(e, scene, project))
                {
                    const auto &meta = AssetManager::GetInstance()->GetMetaData(prefabHandle);
                    if (!meta.filepath.empty())
                    {
                        std::filesystem::path fullPath = project ? project->GetProjectFilepath(meta.filepath) : meta.filepath;
                        prefab->Serialize(fullPath);
                    }

                    SyncAllPrefabInstances(scene, prefabHandle);
                }
            });
        });
    }
} // namespace ignite
