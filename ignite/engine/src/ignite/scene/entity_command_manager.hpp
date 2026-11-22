// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_COMMAND_MANAGER_HPP
#define IGN_ENTITY_COMMAND_MANAGER_HPP

#include "ignite/core/command.hpp"
#include "entity_snapshot.hpp"

#include <string>

namespace ignite
{
    class EntityManagerCommand : public ICommand
    {
    public:
        EntityManagerCommand(const CommandFunc &createFunc, const CommandFunc &destroyFunc, CommandState state, std::string name = "")
            : m_CreateFunc(createFunc)
            , m_DestroyFunc(destroyFunc)
            , m_State(state)
            , m_Name(std::move(name))
        {
        }

        virtual void Execute() override
        {
            switch (m_State)
            {
            case CommandState_Create:
            {
                if (m_CreateFunc) m_CreateFunc();
                break;
            }
            case CommandState_Destroy:
            {
                if (m_DestroyFunc) m_DestroyFunc();
                break;
            }
            default:
                break;
            }
        }

        virtual void Undo() override
        {
            switch (m_State)
            {
            case CommandState_Create:
            {
                if (m_DestroyFunc) m_DestroyFunc();
                break;
            }
            case CommandState_Destroy:
            {
                if (m_CreateFunc) m_CreateFunc();
                break;
            }
            default:
                break;
            }
        }

        std::string GetName() const override
        {
            if (!m_Name.empty())
                return m_Name;
            return m_State == CommandState_Destroy ? "Destroy Entity" : "Create Entity";
        }

        size_t GetMemoryUsage() const override
        {
            return sizeof(*this) + m_Name.capacity();
        }

    private:
        CommandFunc m_CreateFunc;
        CommandFunc m_DestroyFunc;
        CommandState m_State;
        std::string m_Name;
    };

    class EntityCreateCommand : public ICommand
    {
    public:
        EntityCreateCommand(Scene *scene, Entity createdEntity, std::string actionName = "")
            : m_Scene(scene)
            , m_Snapshot(CaptureEntitySubtreeSnapshot(scene, createdEntity))
            , m_Name(std::move(actionName))
        {
            if (m_Name.empty() && !m_Snapshot.rootName.empty())
            {
                m_Name = "Create " + m_Snapshot.rootName;
            }
        }

        void Execute() override
        {
            if (!m_Scene || !m_Snapshot.IsValid())
                return;

            if (!SceneManager::GetEntity(m_Scene, m_Snapshot.rootUUID).IsValid())
            {
                RestoreEntitySubtreeSnapshot(m_Scene, m_Snapshot);
            }
        }

        void Undo() override
        {
            if (!m_Scene || m_Snapshot.rootUUID == UUID(0))
                return;

            Entity existing = SceneManager::GetEntity(m_Scene, m_Snapshot.rootUUID);
            if (existing.IsValid())
            {
                // Refresh snapshot right before destroying so components added or edited right after creation stay intact on Redo.
                EntitySubtreeSnapshot refreshed = CaptureEntitySubtreeSnapshot(m_Scene, existing);
                if (refreshed.IsValid())
                {
                    m_Snapshot = std::move(refreshed);
                }
                SceneManager::DestroyEntity(m_Scene, existing);
            }
        }

        std::string GetName() const override
        {
            return m_Name.empty() ? "Create Entity" : m_Name;
        }

        size_t GetMemoryUsage() const override
        {
            return sizeof(*this) + m_Name.capacity() + m_Snapshot.GetMemoryUsage();
        }

    private:
        Scene *m_Scene = nullptr;
        EntitySubtreeSnapshot m_Snapshot;
        std::string m_Name;
    };
}

#endif