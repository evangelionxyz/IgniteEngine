// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ENTITY_HPP
#define IGN_ENTITY_HPP

#include "component.hpp"
#include "ignite/core/base.hpp"
#include "scene.hpp"

#include <entt/entt.hpp>

namespace ignite
{
    class IGN_API Entity
    {
    public:
        Entity();
        Entity(entt::entity e, Scene *scene);
        Entity(const Entity &other);

        ~Entity();

        template<typename T, typename... Args>
        T &AddComponent(Args &&... args)
        {
            T &comp = m_Scene->registry->get_or_emplace<T>(m_Handle, std::forward<Args>(args)...);

            if (std::is_base_of_v<IComponent, T>)
            {
                m_Scene->OnComponentAdded<T>(*this, comp);
            }

            return comp;
        }

        template<typename T, typename... Args>
        T &AddOrReplaceComponent(Args &&... args)
        {
            T &comp = m_Scene->registry->emplace_or_replace<T>(m_Handle, std::forward<Args>(args)...);

            if (std::is_base_of_v<IComponent, T>)
            {
                m_Scene->OnComponentAdded<T>(*this, comp);
            }
            return comp;
        }

        template<typename T>
        T &GetComponent()
        {
            return m_Scene->registry->get<T>(m_Handle);
        }

        template<typename T>
        bool HasComponent() const
        {
            return m_Scene->registry->all_of<T>(m_Handle);
        }

        template<typename T>
        void RemoveComponent() const
        {
            T &comp = m_Scene->registry->get<T>(m_Handle);
            m_Scene->registry->remove<T>(m_Handle);
        }

        bool IsValid() const
        {
            if (!m_Scene || !m_Scene->registry)
                return false;

            return m_Scene->registry->valid(m_Handle);
        }

        operator bool() const { return IsValid(); }

        operator entt::entity() const { return m_Handle; }
        operator int32_t() const { return static_cast<int32_t>(m_Handle); }
        operator uint32_t() const { return static_cast<uint32_t>(m_Handle); }
        operator uint64_t() const { return static_cast<uint64_t>(m_Handle); }

        bool operator==(const Entity &other) const { return other.m_Handle == m_Handle && other.m_Scene == m_Scene; }
        bool operator!=(const Entity &other) const { return !(*this == other); }

        UUID GetUUID() { return GetComponent<IDComponent>().uuid; }
        UUID GetUUID() const { return const_cast<Entity *>(this)->GetComponent<IDComponent>().uuid; }
        UUID GetParentUUID() { return GetComponent<IDComponent>().parent; }
        UUID GetParentUUID() const { return const_cast<Entity *>(this)->GetComponent<IDComponent>().parent; }
        TransformComponent &GetTransform() { return GetComponent<TransformComponent>(); }
        const TransformComponent &GetTransform() const { return const_cast<Entity *>(this)->GetComponent<TransformComponent>(); }
        const std::string &GetName() { return GetComponent<IDComponent>().name; }
        const std::string &GetName() const { return const_cast<Entity *>(this)->GetComponent<IDComponent>().name; }

    private:
        entt::entity m_Handle;
        Scene *m_Scene;
    };
}

#endif
