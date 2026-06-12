// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ENTITY_PROPERTY_COMMAND_HPP
#define IGN_ENTITY_PROPERTY_COMMAND_HPP

#include "ignite/core/command.hpp"
#include "scene_manager.hpp"
#include "entity.hpp"
#include <vector>

namespace ignite
{
    // ---------------------------------------------------------------------------
    // ComponentPropertyCommand<TComponent>
    //
    // Records a before/after snapshot of an entire component for undo/redo.
    // Works for any copyable component type.
    //
    // Typical usage inside RenderInspector() (drag-type widgets):
    //
    //   static TransformComponent s_Before;
    //   if (ImGui::IsItemActivated())                 // user started dragging
    //       s_Before = comp;
    //
    //   ImGui::DragFloat3("Translation", &comp.localTranslation.x, 0.025f);
    //
    //   if (ImGui::IsItemDeactivatedAfterEdit())      // user released the widget
    //   {
    //       CommandManager::AddCommand(
    //           CreateScope<ComponentPropertyCommand<TransformComponent>>(
    //               m_Scene.get(), selectedEntity.GetUUID(), s_Before, comp));
    //   }
    //
    // For instant-commit widgets (Checkbox, Combo, etc.):
    //
    //   TransformComponent before = comp;
    //   if (ImGui::Checkbox("Visible", &comp.visible))
    //   {
    //       CommandManager::AddCommand(
    //           CreateScope<ComponentPropertyCommand<TransformComponent>>(
    //               m_Scene.get(), selectedEntity.GetUUID(), before, comp));
    //   }
    // ---------------------------------------------------------------------------

    template<typename TComponent>
    class ComponentPropertyCommand : public ICommand
    {
    public:
        ComponentPropertyCommand(Scene *scene, UUID entityUUID,
                                  TComponent oldValue, TComponent newValue)
            : m_Scene(scene)
            , m_UUID(entityUUID)
            , m_OldValue(std::move(oldValue))
            , m_NewValue(std::move(newValue))
        {
        }

        void Execute() override { Apply(m_NewValue); }
        void Undo()    override { Apply(m_OldValue); }

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
    };

    // ---------------------------------------------------------------------------
    // ComponentPropertyBatchCommand<TComponent>
    //
    // Records multiple property changes of the same component type as a single command.
    // Useful for multi-entity gizmo manipulations.
    // ---------------------------------------------------------------------------
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

        ComponentPropertyBatchCommand(Scene *scene, std::vector<Entry> entries)
            : m_Scene(scene)
            , m_Entries(std::move(entries))
        {
        }

        void Execute() override { for (auto &e : m_Entries) Apply(e.uuid, e.newValue); }
        void Undo()    override { for (auto &e : m_Entries) Apply(e.uuid, e.oldValue); }

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
    };
}

#endif