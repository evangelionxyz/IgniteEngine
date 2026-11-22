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
            , m_Snapshot(CaptureEntitySubtreeSnapshot(scene, entity))
        {
        }

        // Execute / Redo — destroy the root entity (children are destroyed recursively)
        void Execute() override
        {
            if (m_Scene && m_Snapshot.rootUUID != UUID(0))
            {
                Entity e = SceneManager::GetEntity(m_Scene, m_Snapshot.rootUUID);
                if (e.IsValid())
                    SceneManager::DestroyEntity(m_Scene, e);
            }
        }

        // Undo — recreate the full subtree with all components & sibling order
        void Undo() override
        {
            if (m_Scene && m_Snapshot.IsValid() && !SceneManager::GetEntity(m_Scene, m_Snapshot.rootUUID).IsValid())
            {
                RestoreEntitySubtreeSnapshot(m_Scene, m_Snapshot);
            }
        }

        std::string GetName() const override
        {
            return m_Snapshot.rootName.empty() ? "Delete Entity" : ("Delete " + m_Snapshot.rootName);
        }

        size_t GetMemoryUsage() const override
        {
            return sizeof(*this) + m_Snapshot.GetMemoryUsage();
        }

    private:
        Scene *m_Scene = nullptr;
        EntitySubtreeSnapshot m_Snapshot;
    };
}

#endif