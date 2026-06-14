// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_REPARENT_COMMAND_HPP
#define IGN_ENTITY_REPARENT_COMMAND_HPP

#include "ignite/core/command.hpp"
#include "scene_manager.hpp"
#include "entity.hpp"
#include "component.hpp"

namespace ignite
{
    // ---------------------------------------------------------------------------
    // EntityReparentCommand — records a parent change (drag-drop in hierarchy).
    //
    // Usage:
    //   UUID oldParent = src.GetComponent<IDComponent>().parent;
    //   SceneManager::AddChild(scene, newParent, src);          // do the change
    //   CommandManager::AddCommand(
    //       CreateScope<EntityReparentCommand>(scene, srcUUID, oldParent, newParentUUID));
    //
    // Pass UUID(0) for oldParentUUID / newParentUUID when the entity is / becomes a root.
    // ---------------------------------------------------------------------------
    class EntityReparentCommand : public ICommand
    {
    public:
        EntityReparentCommand(Scene *scene, UUID sourceUUID,
            UUID oldParentUUID, UUID newParentUUID)
            : m_Scene(scene)
            , m_Source(sourceUUID)
            , m_OldParent(oldParentUUID)
            , m_NewParent(newParentUUID)
        {
        }

        void Execute() override { Reparent(m_NewParent); }
        void Undo()    override { Reparent(m_OldParent); }

    private:
        void Reparent(UUID targetParentUUID)
        {
            Entity src = SceneManager::GetEntity(m_Scene, m_Source);
            if (!src.IsValid())
                return;

            IDComponent &srcId = src.GetComponent<IDComponent>();

            // Detach from current parent
            if (srcId.parent != UUID(0))
            {
                Entity oldP = SceneManager::GetEntity(m_Scene, srcId.parent);
                if (oldP.IsValid())
                    oldP.GetComponent<IDComponent>().RemoveChild(srcId.uuid);
            }
            srcId.parent = UUID(0);

            // Attach to new parent (or leave as root if UUID is 0)
            if (targetParentUUID != UUID(0))
            {
                Entity newP = SceneManager::GetEntity(m_Scene, targetParentUUID);
                if (newP.IsValid())
                    SceneManager::AddChild(m_Scene, newP, src);
            }

            m_Scene->SetDirtyFlag(true);
        }

        Scene *m_Scene;
        UUID   m_Source;
        UUID   m_OldParent;
        UUID   m_NewParent;
    };
}

#endif