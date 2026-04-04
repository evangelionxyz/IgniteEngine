/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "scene_manager.hpp"
#include "scene.hpp"
#include "entity.hpp"
#include "entity_command_manager.hpp"
#include "entity_destroy_command.hpp"

#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/jolt/jolt_physics.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/uuid.hpp"

#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"

#include "ignite/math/math.hpp"

#include <string>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace ignite
{    
    static std::string GenerateUniqueName(const std::string &name, const std::vector<std::string> &names, std::unordered_map<std::string, uint32_t> &strMap)
    {
        // iterate names for the first time
        // check if the name already exists
        bool found = false;
        for (auto &n : names)
        {
            found = n == name;
            if (found)
                break;
        }

        // if the name does not exist, return it as the unique key
        if (!found)
            return name;

        // generate a unique key if the name already exists
        i32 counter = 1;
        std::string uniqueKey;

        bool unique = false;
        while (!unique)
        {
            uniqueKey = fmt::format("{} ({})", name, counter);
            unique = true;

            // iterate once again
            // check if they are already unique
            for (auto &n : names)
            {
                if (n == uniqueKey) // not unique continue search
                {
                    unique = false;
                    break;
                }
            }
            counter++; // increment the counter if names are not unique
        }

        // update the counter to reflect the highest number used
        strMap[name] = counter - 1;

        // cleanup counters for names that no longer exist
        for (auto it = strMap.begin(); it != strMap.end();)
        {
            bool exists = false;
            for (auto &n : names)
            {
                exists = name.find(it->first) == 0;
                if (exists)
                    break;
            }

            if (!exists)
                it = strMap.erase(it);
            else
                ++it;
        }

        return uniqueKey;
    }

    Entity SceneManager::CreateEntity(Scene *scene, const std::string &name, EntityType type, UUID uuid)
    {
        scene->SetDirtyFlag(true);
        Entity entity = Entity { scene->registry->create(), scene };
        entity.AddComponent<IDComponent>(name, type, uuid);
        entity.AddComponent<TransformComponent>(TransformComponent({0.0f, 0.0f, 0.0f}));
        scene->entities[uuid] = entity;
        return entity;
    }

    Entity SceneManager::CreateSprite(Scene *scene, const std::string &name, UUID uuid)
    {
        // create local storage for entity data
        Entity createdEntity;

        // prepare entity creation logic
        std::function createFunc = [=, &createdEntity]() mutable
        {
            createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
            createdEntity.AddComponent<Sprite2DComponent>();
        };

        // immediately call createFunc to initialize createdEntity
        createFunc();

        // capture scene and entity by value to preserve the for undo
        Scene *capturedScene = scene;
        UUID capturedUUID = createdEntity.GetComponent<IDComponent>().uuid;

        std::function destroyFunc = [capturedScene, capturedUUID]()
        {
            if (Entity entityToDestroy = GetEntity(capturedScene, capturedUUID))
            {
                DestroyEntity(capturedScene, entityToDestroy);
            }
        };

        CommandManager::AddCommand(
            CreateScope<EntityManagerCommand>(
                createFunc, 
                destroyFunc, 
                CommandState_Create
            )
        );
       
        return createdEntity;
    }

	Entity SceneManager::CreateCircle(Scene *scene, const std::string &name, UUID uuid)
	{
		// create local storage for entity data
		Entity createdEntity;

		// prepare entity creation logic
		std::function createFunc = [=, &createdEntity]() mutable
			{
				createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
				createdEntity.AddComponent<Circle2DComponent>();
			};

		// immediately call createFunc to initialize createdEntity
		createFunc();

		// capture scene and entity by value to preserve the for undo
		Scene *capturedScene = scene;
		UUID capturedUUID = createdEntity.GetComponent<IDComponent>().uuid;

		std::function destroyFunc = [capturedScene, capturedUUID]()
			{
				if (Entity entityToDestroy = GetEntity(capturedScene, capturedUUID))
				{
					DestroyEntity(capturedScene, entityToDestroy);
				}
			};

		CommandManager::AddCommand(
			CreateScope<EntityManagerCommand>(
				createFunc,
				destroyFunc,
				CommandState_Create
			)
		);

		return createdEntity;
	}

	Entity SceneManager::CreatePointLight2D(Scene *scene, const std::string &name, UUID uuid)
	{
		// create local storage for entity data
		Entity createdEntity;

		// prepare entity creation logic
		std::function createFunc = [=, &createdEntity]() mutable
			{
				createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
				createdEntity.AddComponent<PointLight2DComponent>();
			};

		// immediately call createFunc to initialize createdEntity
		createFunc();

		// capture scene and entity by value to preserve the for undo
		Scene *capturedScene = scene;
		UUID capturedUUID = createdEntity.GetComponent<IDComponent>().uuid;

		std::function destroyFunc = [capturedScene, capturedUUID]()
			{
				if (Entity entityToDestroy = GetEntity(capturedScene, capturedUUID))
				{
					DestroyEntity(capturedScene, entityToDestroy);
				}
			};

		CommandManager::AddCommand(
			CreateScope<EntityManagerCommand>(
				createFunc,
				destroyFunc,
				CommandState_Create
			)
		);

		return createdEntity;
	}

	Entity SceneManager::CreateMesh(Scene *scene, const std::string &name, UUID uuid)
    {
        scene->SetDirtyFlag(true);

        Entity entity = Entity { scene->registry->create(), scene };
        entity.AddComponent<IDComponent>(name, EntityType_Node, uuid);
        entity.AddComponent<TransformComponent>(TransformComponent({ 0.0f, 0.0f, 0.0f }));

        scene->entities[uuid] = entity;

        return entity;
    }

    Entity SceneManager::CreateCamera(Scene* scene, const std::string& name, UUID uuid)
    {
        // create local storage for entity data
        Entity createdEntity;

        // prepare entity creation logic
        std::function createFunc = [=, &createdEntity]() mutable
        {
            createdEntity = CreateEntity(scene, name, EntityType_Camera, uuid);
            createdEntity.AddComponent<CameraComponent>();
        };

        // immediately call createFunc to initialize createdEntity
        createFunc();

        // capture scene and entity by value to preserve the for undo
        Scene *capturedScene = scene;
        UUID capturedUUID = createdEntity.GetComponent<IDComponent>().uuid;

        std::function destroyFunc = [capturedScene, capturedUUID]()
        {
            if (Entity entityToDestroy = GetEntity(capturedScene, capturedUUID))
            {
                DestroyEntity(capturedScene, entityToDestroy);
            }
        };

        CommandManager::AddCommand(
            CreateScope<EntityManagerCommand>(
                createFunc, 
                destroyFunc, 
                CommandState_Create
            )
        );
       
        return createdEntity;
    }

    Entity SceneManager::CreateWorldEnvironment(Scene *scene, const std::string &name, UUID uuid)
    {
        // create local storage for entity data
        Entity createdEntity;

        // prepare entity creation logic
        std::function createFunc = [=, &createdEntity]() mutable
        {
            createdEntity = CreateEntity(scene, name, EntityType_WorldEnvironment, uuid);
            createdEntity.AddComponent<WorldEnvironment>();
        };

        // immediately call createFunc to initialize createdEntity
        createFunc();

        // capture scene and entity by value to preserve the for undo
        Scene *capturedScene = scene;
        UUID capturedUUID = createdEntity.GetComponent<IDComponent>().uuid;

        std::function destroyFunc = [capturedScene, capturedUUID]()
        {
            if (Entity entityToDestroy = GetEntity(capturedScene, capturedUUID))
            {
                DestroyEntity(capturedScene, entityToDestroy);
            }
        };

        CommandManager::AddCommand(
            CreateScope<EntityManagerCommand>(
                createFunc, 
                destroyFunc, 
                CommandState_Create
            )
        );

        return createdEntity;
    }

    Entity SceneManager::CreateEmptyEntity(Scene *scene, const std::string &name, UUID uuid)
    {
        // create local storage for entity data
        Entity createdEntity;

        // prepare entity creation logic
        std::function createFunc = [=, &createdEntity]() mutable
        {
            createdEntity = CreateEntity(scene, name, EntityType_Node, uuid);
        };

        // immediately call createFunc to initialize createdEntity
        createFunc();

        // capture by value for undo safety
        Scene *capturedScene = scene;
        UUID capturedUUID = createdEntity.GetComponent<IDComponent>().uuid;

        std::function destroyFunc = [capturedScene, capturedUUID]()
        {
            if (Entity entityToDestroy = GetEntity(capturedScene, capturedUUID))
                DestroyEntity(capturedScene, entityToDestroy);
        };

        CommandManager::AddCommand(
            CreateScope<EntityManagerCommand>(
                createFunc,
                destroyFunc,
                CommandState_Create
            )
        );

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
        scene->SetDirtyFlag(true);

        if (!scene || !scene->registry->valid(entity))
            return;

		scene->physics->DestroyEntity(entity);
		scene->physics2D->DestroyEntity(entity);

        IDComponent idComp = entity.GetComponent<IDComponent>();

        // recursively destroy children
        for (UUID childId : idComp.children)
        {
            entity.GetComponent<IDComponent>().RemoveChild(childId);
            DestroyEntity(scene, GetEntity(scene, childId));
        }

        scene->registry->destroy(entity);
        scene->entities.erase(idComp.uuid);
        
        // remove from parent
        if (idComp.parent != UUID(0))
        {
            Entity parent = SceneManager::GetEntity(scene, idComp.parent);
            parent.GetComponent<IDComponent>().RemoveChild(idComp.uuid);
        }
    }

    void SceneManager::DestroyEntity(Scene *scene, UUID uuid)
    {
        DestroyEntity(scene, GetEntity(scene, uuid));
    }

    Entity SceneManager::DuplicateEntity(Scene *scene, Entity entity, bool addToParent)
    {
        scene->SetDirtyFlag(true);

        // first, get current entity's ID Component
        IDComponent &idComp = entity.GetComponent<IDComponent>();

        Entity newEntity = SceneManager::CreateEntity(scene, idComp.name, idComp.type);

        // copy current entity's components to new entity
        SceneManager::CopyComponentIfExists(AllComponents{}, newEntity, entity);

        // get new entity's ID Component
        IDComponent &newEntityIDComp = newEntity.GetComponent<IDComponent>();

        // create its children
        for (UUID cid : idComp.children)
        {
            Entity newChildEntity = DuplicateEntity(scene, GetEntity(scene, cid), false); // add to parent false

            IDComponent &childId = newChildEntity.GetComponent<IDComponent>();
            
            // add this child to new entity
            newEntityIDComp.AddChild(childId.uuid);

            // set child parent to new entity
            childId.parent = newEntityIDComp.uuid;
        }

        // check if current entity has a parent
        if (idComp.parent != 0 && addToParent)
        {
            // get the current entity's parent
            Entity parent = GetEntity(scene, idComp.parent);
            IDComponent &parentIDComp = parent.GetComponent<IDComponent>();

            // set new entity parent to this parent
            newEntityIDComp.parent = parentIDComp.uuid;

            // add child to parent
            parentIDComp.AddChild(newEntityIDComp.uuid);
        }

        if (scene->IsRunning())
        {
		    scene->physics->InstantiateEntity(newEntity);
		    scene->physics2D->InstantiateEntity(newEntity);
        }

        if (!scene->IsRunning())
        {
			// Capture the new entity UUID so undo can destroy it
			UUID newUUID = newEntityIDComp.uuid;
			CommandManager::AddCommand(
				CreateScope<EntityManagerCommand>(
					[scene, newUUID]() { /* no-op: already duplicated */ },
					[scene, newUUID]()
					{
						if (Entity e = GetEntity(scene, newUUID))
							DestroyEntity(scene, e);
					},
					CommandState_Create
				)
			);
        }

        return newEntity;
    }

    Entity SceneManager::GetEntity(Scene *scene, UUID uuid)
    {
        if (scene->entities.contains(uuid))
            return Entity { scene->entities[uuid], scene };

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

    void SceneManager::AddChild(Scene *scene, Entity destination, Entity source)
    {
        scene->SetDirtyFlag(true);

        IDComponent &destIDComp = destination.GetComponent<IDComponent>();
        IDComponent &sourceIDComp = source.GetComponent<IDComponent>();

        if (!IsParent(scene, destIDComp.uuid, sourceIDComp.uuid))
        {
            // remove from current parent
            if (sourceIDComp.parent != 0)
            {
                Entity currentParent = GetEntity(scene, sourceIDComp.parent);
                currentParent.GetComponent<IDComponent>().RemoveChild(sourceIDComp.uuid);
            }

            // add to target parent
            destIDComp.AddChild(sourceIDComp.uuid);
            sourceIDComp.parent = destIDComp.uuid;
        }
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
        // create new scene with other's name
        Ref<Scene> newScene = CreateRef<Scene>(other->GetProject(), other->name);

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

        // copy scene extra data
        newScene->handle = other->handle;
		newScene->Resize(other->m_ViewportWidth, other->m_ViewportHeight);

        // Do not copy entities (it is created when creating entity)
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

        DeviceManager::GetInstance()->WaitForIdle();

        return newScene;
    }
} // namespace ignite
