// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_PROPERTY_COMMAND_HPP
#define IGN_ENTITY_PROPERTY_COMMAND_HPP

#include "ignite/core/command.hpp"
#include "scene_manager.hpp"
#include "entity.hpp"
#include <string>
#include <vector>

namespace ignite
{
    template<typename TComponent>
    class ComponentPropertyCommand : public ICommand
    {
    public:
        ComponentPropertyCommand(Scene *scene, UUID entityUUID,
                                  TComponent oldValue, TComponent newValue,
                                  std::string name = "Edit Component")
            : m_Scene(scene)
            , m_UUID(entityUUID)
            , m_OldValue(std::move(oldValue))
            , m_NewValue(std::move(newValue))
            , m_Name(std::move(name))
        {
        }

        void Execute() override { Apply(m_NewValue); }
        void Undo()    override { Apply(m_OldValue); }

        std::string GetName() const override { return m_Name.empty() ? "Edit Component" : m_Name; }
        size_t GetMemoryUsage() const override { return sizeof(*this) + m_Name.capacity(); }

    private:
        void Apply(const TComponent &value)
        {
            Entity e = SceneManager::GetEntity(m_Scene, m_UUID);
            if (e.IsValid() && e.HasComponent<TComponent>())
            {
                e.GetComponent<TComponent>() = value;
                m_Scene->SetDirtyFlag(true);
            }
        }

        Scene     *m_Scene;
        UUID       m_UUID;
        TComponent m_OldValue;
        TComponent m_NewValue;
        std::string m_Name;
    };

    template<typename TComponent>
    class ComponentPropertyBatchCommand : public ICommand
    {
    public:
        struct Entry
        {
            UUID       uuid;
            TComponent oldValue;
            TComponent newValue;
        };

        ComponentPropertyBatchCommand(Scene *scene, std::vector<Entry> entries, std::string name = "Edit Components")
            : m_Scene(scene)
            , m_Entries(std::move(entries))
            , m_Name(std::move(name))
        {
        }

        void Execute() override { for (auto &e : m_Entries) Apply(e.uuid, e.newValue); }
        void Undo()    override { for (auto &e : m_Entries) Apply(e.uuid, e.oldValue); }

        std::string GetName() const override { return m_Name.empty() ? "Edit Components" : m_Name; }
        size_t GetMemoryUsage() const override
        {
            return sizeof(*this) + m_Name.capacity() + m_Entries.capacity() * sizeof(Entry);
        }

    private:
        void Apply(UUID uuid, const TComponent &value)
        {
            Entity e = SceneManager::GetEntity(m_Scene, uuid);
            if (e.IsValid() && e.HasComponent<TComponent>())
            {
                e.GetComponent<TComponent>() = value;
                m_Scene->SetDirtyFlag(true);
            }
        }

        Scene             *m_Scene;
        std::vector<Entry> m_Entries;
        std::string        m_Name;
    };

    template<typename TComponent>
    class ComponentAddCommand : public ICommand
    {
    public:
        ComponentAddCommand(Scene *scene, UUID entityUUID, std::string compName = "Component")
            : m_Scene(scene)
            , m_UUID(entityUUID)
            , m_CompName(std::move(compName))
        {
        }

        void Execute() override
        {
            Entity e = SceneManager::GetEntity(m_Scene, m_UUID);
            if (!e.IsValid())
                return;

            if (!e.HasComponent<TComponent>())
            {
                if (m_HasSnapshot)
                    e.AddOrReplaceComponent<TComponent>(m_Snapshot);
                else
                    e.AddComponent<TComponent>();
                m_Scene->SetDirtyFlag(true);
            }
        }

        void Undo() override
        {
            Entity e = SceneManager::GetEntity(m_Scene, m_UUID);
            if (!e.IsValid() || !e.HasComponent<TComponent>())
                return;

            m_Snapshot = e.GetComponent<TComponent>();
            m_HasSnapshot = true;
            e.RemoveComponent<TComponent>();
            m_Scene->SetDirtyFlag(true);
        }

        std::string GetName() const override { return "Add " + m_CompName; }
        size_t GetMemoryUsage() const override { return sizeof(*this) + m_CompName.capacity(); }

    private:
        Scene *m_Scene = nullptr;
        UUID m_UUID = UUID(0);
        std::string m_CompName;
        TComponent m_Snapshot{};
        bool m_HasSnapshot = false;
    };

    template<typename TComponent>
    class ComponentRemoveCommand : public ICommand
    {
    public:
        ComponentRemoveCommand(Scene *scene, UUID entityUUID, TComponent oldValue, std::string compName = "Component")
            : m_Scene(scene)
            , m_UUID(entityUUID)
            , m_OldValue(std::move(oldValue))
            , m_CompName(std::move(compName))
        {
        }

        void Execute() override
        {
            Entity e = SceneManager::GetEntity(m_Scene, m_UUID);
            if (e.IsValid() && e.HasComponent<TComponent>())
            {
                m_OldValue = e.GetComponent<TComponent>();
                e.RemoveComponent<TComponent>();
                m_Scene->SetDirtyFlag(true);
            }
        }

        void Undo() override
        {
            Entity e = SceneManager::GetEntity(m_Scene, m_UUID);
            if (e.IsValid())
            {
                e.AddOrReplaceComponent<TComponent>(m_OldValue);
                m_Scene->SetDirtyFlag(true);
            }
        }

        std::string GetName() const override { return "Remove " + m_CompName; }
        size_t GetMemoryUsage() const override { return sizeof(*this) + m_CompName.capacity(); }

    private:
        Scene *m_Scene = nullptr;
        UUID m_UUID = UUID(0);
        TComponent m_OldValue;
        std::string m_CompName;
    };
}

#endif