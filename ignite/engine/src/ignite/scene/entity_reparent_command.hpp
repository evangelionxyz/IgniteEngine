// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_REPARENT_COMMAND_HPP
#define IGN_ENTITY_REPARENT_COMMAND_HPP

#include "ignite/core/command.hpp"
#include "scene_manager.hpp"
#include "entity.hpp"
#include "component.hpp"

#include <limits>
#include <string>
#include <vector>

namespace ignite
{
    struct HierarchyMoveEntry
    {
        UUID entityUUID = UUID(0);
        UUID oldParent = UUID(0);
        size_t oldIndex = 0;
        UUID newParent = UUID(0);
        size_t newIndex = 0;
    };

    class EntityHierarchyMoveCommand : public ICommand
    {
    public:
        EntityHierarchyMoveCommand(Scene *scene, std::vector<HierarchyMoveEntry> entries, std::string name = "Reorder Hierarchy")
            : m_Scene(scene)
            , m_Entries(std::move(entries))
            , m_Name(std::move(name))
        {
        }

        void Execute() override
        {
            for (const HierarchyMoveEntry &entry : m_Entries)
            {
                SceneManager::MoveEntity(m_Scene, entry.entityUUID, entry.newParent, entry.newIndex);
            }
        }

        void Undo() override
        {
            for (auto it = m_Entries.rbegin(); it != m_Entries.rend(); ++it)
            {
                SceneManager::MoveEntity(m_Scene, it->entityUUID, it->oldParent, it->oldIndex);
            }
        }

        std::string GetName() const override { return m_Name; }

        size_t GetMemoryUsage() const override
        {
            return sizeof(*this) + m_Name.capacity() + m_Entries.capacity() * sizeof(HierarchyMoveEntry);
        }

    private:
        Scene *m_Scene = nullptr;
        std::vector<HierarchyMoveEntry> m_Entries;
        std::string m_Name;
    };

    // ---------------------------------------------------------------------------
    // EntityReparentCommand — records a parent / sibling order change.
    // ---------------------------------------------------------------------------
    class EntityReparentCommand : public ICommand
    {
    public:
        EntityReparentCommand(Scene *scene, UUID sourceUUID,
            UUID oldParentUUID, UUID newParentUUID,
            size_t oldIndex = (std::numeric_limits<size_t>::max)(),
            size_t newIndex = (std::numeric_limits<size_t>::max)())
            : m_Scene(scene)
            , m_Source(sourceUUID)
            , m_OldParent(oldParentUUID)
            , m_NewParent(newParentUUID)
            , m_OldIndex(oldIndex)
            , m_NewIndex(newIndex)
        {
        }

        void Execute() override { SceneManager::MoveEntity(m_Scene, m_Source, m_NewParent, m_NewIndex); }
        void Undo()    override { SceneManager::MoveEntity(m_Scene, m_Source, m_OldParent, m_OldIndex); }

        std::string GetName() const override { return "Reparent Entity"; }
        size_t GetMemoryUsage() const override { return sizeof(*this); }

    private:
        Scene *m_Scene;
        UUID   m_Source;
        UUID   m_OldParent;
        UUID   m_NewParent;
        size_t m_OldIndex;
        size_t m_NewIndex;
    };
}

#endif