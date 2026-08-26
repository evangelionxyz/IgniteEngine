// Copyright (c) 2026 Evangelion Manuhutu

#include "entity.hpp"

namespace ignite
{
    Entity::Entity()
        : m_Handle(entt::null)
        , m_Scene(nullptr)
        {}
    
    Entity::Entity(const entt::entity e, Scene *scene)
        : m_Handle(e)
        , m_Scene(scene)
        {}

    Entity::Entity(const Entity &other)
        : m_Handle(other.m_Handle)
        , m_Scene(other.m_Scene)
    {
    }

    Entity::~Entity()
    {
        m_Handle = { entt::null };
        m_Scene = nullptr;
    }
}
