// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_DESTROY_COMMAND_HPP
#define IGN_ENTITY_DESTROY_COMMAND_HPP

#include "ignite/core/command.hpp"
#include "scene_manager.hpp"
#include "entity_snapshot.hpp"

#include <vector>

namespace ignite
{
    // ---------------------------------------------------------------------------
    // EntityDestroyCommand — destroys an entity (and its whole subtree) while
    // keeping enough state to recreate everything on Undo.
    //
    // Call BEFORE actually destroying:
    //   CommandManager::ExecuteCommand(
    //       CreateScope<EntityDestroyCommand>(scene, entity));
    //
    // Execute() does the actual destroy.
    // Undo() recreates the entity and its children from their snapshots.
    // ---------------------------------------------------------------------------
    class EntityDestroyCommand : public ICommand
    {
    public:
        EntityDestroyCommand(Scene *scene, Entity entity)
            : m_Scene(scene)
        {
            // Snapshot the entity and all its descendants BEFORE destruction
            SnapshotSubtree(entity);
        }

        // Redo — destroy the root entity (children are destroyed recursively)
        void Execute() override
        {
            if (!m_Snapshots.empty())
            {
                Entity e = SceneManager::GetEntity(m_Scene, m_Snapshots[0].uuid);
                if (e.IsValid())
                    SceneManager::DestroyEntity(m_Scene, e);
            }
        }

        // Undo — restore in snapshot order (parent before children)
        void Undo() override
        {
            for (const EntitySnapshot &snap : m_Snapshots)
            {
                // Only restore if it doesn't already exist (safety check)
                if (!SceneManager::GetEntity(m_Scene, snap.uuid).IsValid())
                    RestoreEntity(m_Scene, snap);
            }

            // Re-link parent for the root entity of this subtree
            if (!m_Snapshots.empty())
            {
                const EntitySnapshot &root = m_Snapshots[0];
                if (root.parent != UUID(0))
                {
                    Entity parentEntity = SceneManager::GetEntity(m_Scene, root.parent);
                    Entity restoredRoot = SceneManager::GetEntity(m_Scene, root.uuid);
                    if (parentEntity.IsValid() && restoredRoot.IsValid())
                    {
                        IDComponent &parentId = parentEntity.GetComponent<IDComponent>();
                        // Add child reference if not already present
                        bool alreadyChild = false;
                        for (UUID c : parentId.children)
                            if (c == root.uuid) { alreadyChild = true; break; }
                        if (!alreadyChild)
                            parentId.AddChild(root.uuid);
                    }
                }
            }
        }

    private:
        // DFS pre-order: parent snapshot comes before child snapshots
        void SnapshotSubtree(Entity entity)
        {
            if (!entity.IsValid())
                return;

            m_Snapshots.push_back(SnapshotEntity(m_Scene, entity));

            const IDComponent &id = entity.GetComponent<IDComponent>();
            for (UUID childUUID : id.children)
            {
                Entity child = SceneManager::GetEntity(m_Scene, childUUID);
                SnapshotSubtree(child);
            }
        }

        Scene                    *m_Scene;
        std::vector<EntitySnapshot> m_Snapshots; // [0] = root, rest = descendants
    };
}

#endif