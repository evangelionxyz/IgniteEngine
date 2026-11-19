// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include "ignite/core/base.hpp"
#include "component_group.hpp"
#include "ignite/core/uuid.hpp"
#include "entity.hpp"
#include <limits>
#include <type_traits>

namespace ignite
{
    class Prefab;
    enum class EMeshType;

    enum class HierarchyDropPosition : uint8_t
    {
        Above = 0,
        Inside,
        Below
    };

    class IGN_API SceneManager
    {
    public:
        static Entity CreateEntity(Scene *scene, const std::string &name, EntityType type, UUID uuid = UUID());
        static Entity CreateFolder(Scene *scene, const std::string &name = "Folder", UUID uuid = UUID());

        static Entity CreateSprite(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateCircle(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreatePointLight2D(Scene *scene, const std::string &name, UUID uuid = UUID());

        static Entity CreateMesh(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreatePrimitive(Scene *scene, EMeshType type, const std::string &name = "", UUID uuid = UUID());
        static Entity CreateCamera(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateWorldEnvironment(Scene *scene, const std::string &name, UUID uuid = UUID());
        static Entity CreateEmptyEntity(Scene *scene, const std::string &name, UUID uuid = UUID());

        static std::string GenerateUniqueEntityName(Scene *scene, const std::string &baseName);
        static void RenameEntity(Scene *scene, Entity entity, const std::string &newName);
        static void DestroyEntity(Scene *scene, Entity entity);
        static void DestroyEntity(Scene *scene, UUID uuid);
        static Entity GetEntity(Scene *scene, UUID uuid);
        static Entity GetEntity(Scene *scene, const std::string &name);
        static Entity DuplicateEntity(Scene *scene, Entity entity, bool addToParent = true, bool recordUndo = true);
        static Entity CloneEntityTree(Scene *destScene, Scene *srcScene, Entity srcRoot);
        static Entity InstantiatePrefab(Scene *scene, const Ref<Prefab> &prefab, Entity parent = {});
        static void SyncAllPrefabInstances(Scene *scene, AssetHandle prefabHandle);
        static void ApplyPrefabChanges(Entity entity, Scene *scene, Project *project);

        static void SyncHierarchyOrder(Scene *scene);
        static size_t GetSiblingIndex(Scene *scene, UUID entityUUID);
        static bool MoveEntity(Scene *scene, UUID entityUUID, UUID newParentUUID, size_t insertIndex = (std::numeric_limits<size_t>::max)());
        static bool ReorderRelativeTo(Scene *scene, UUID sourceUUID, UUID targetUUID, HierarchyDropPosition position);

        static bool AddChild(Scene *scene, Entity destination, Entity source);
        static bool ChildExists(Scene *scene, Entity destination, Entity source);
        static bool IsParent(Scene *scene, UUID target, UUID source);
        static Entity FindChild(Scene *scene, Entity parent, UUID uuid);

        static Ref<Scene> Copy(Ref<Scene> &other);

        using EntityMap = std::unordered_map<UUID, entt::entity>;
        using EntityComponents = std::unordered_map<entt::entity, std::vector<IComponent *>>;

        template<typename... Component>
        static void CopyComponent(entt::registry *destRegistry, entt::registry *srcRegistry, const EntityMap &entityMap)
        {
            ([&]()
                {
                    auto view = srcRegistry->view<Component>();
                    for (auto srcEntity : view)
                    {
                        for (auto [uuid, destEntity] : entityMap)
                        {
                            // key (UUID)
                            if (uuid == srcRegistry->get<IDComponent>(srcEntity).uuid)
                            {
                                destRegistry->emplace_or_replace<Component>(destEntity, srcRegistry->get<Component>(srcEntity));
                            }
                        }
                    }
                }(), ...
            );
        }

        template<typename... Component>
        static void CopyComponent(ComponentGroup<Component...>, entt::registry *destRegistry, entt::registry *srcRegistry, const EntityMap &entityMap)
        {
            CopyComponent<Component...>(destRegistry, srcRegistry, entityMap);
        }

        template <typename... Component>
        static void CopyComponentIfExists(Entity dstEntity, Entity srcEntity)
        {
            ([&]()
            {
                if (srcEntity.HasComponent<Component>())
                {
                    dstEntity.AddOrReplaceComponent<Component>(srcEntity.GetComponent<Component>());
                }
            }(), ...);
        }

        template<typename... Component>
        static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dstEntity, Entity srcEntity)
        {
            CopyComponentIfExists<Component...>(dstEntity, srcEntity);
        }

        static void Transition(AssetHandle nextSceneHandle);
        static void RequestStop();
        static void SetStopCallback(std::function<void()> callback);
        static void ExecutePendingTransition();

    private:
        static bool s_TransitionPending;
        static AssetHandle s_PendingSceneHandle;
        static bool s_StopPending;
        static std::function<void()> s_StopCallback;
    };
}

#endif
