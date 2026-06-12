// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_RENAME_COMMAND_HPP
#define IGN_ENTITY_RENAME_COMMAND_HPP

#include "ignite/core/command.hpp"
#include "scene_manager.hpp"
#include "entity.hpp"

#include <string>

namespace ignite
{
    // ---------------------------------------------------------------------------
    // EntityRenameCommand — records an entity name change so it can be undone.
    // Usage:
    //   CommandManager::ExecuteCommand(
    //       CreateScope<EntityRenameCommand>(scene, entity.GetUUID(), oldName, newName));
    // ---------------------------------------------------------------------------
    class EntityRenameCommand : public ICommand
    {
    public:
        EntityRenameCommand(Scene *scene, UUID entityUUID,
                            const std::string &oldName, const std::string &newName)
            : m_Scene(scene)
            , m_UUID(entityUUID)
            , m_OldName(oldName)
            , m_NewName(newName)
        {
        }

        void Execute() override { Apply(m_NewName); }
        void Undo()    override { Apply(m_OldName); }

    private:
        void Apply(const std::string &name)
        {
            Entity e = SceneManager::GetEntity(m_Scene, m_UUID);
            if (e.IsValid())
                SceneManager::RenameEntity(m_Scene, e, name);
        }

        Scene      *m_Scene;
        UUID        m_UUID;
        std::string m_OldName;
        std::string m_NewName;
    };
}

#endif