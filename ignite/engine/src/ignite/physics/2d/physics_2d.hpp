// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PHYSICS_2D_HPP
#define IGN_PHYSICS_2D_HPP

#include "physics_2d_component.hpp"
#include <box2d/box2d.h>
#include <entt/entt.hpp>
#include <ignite/core/types.hpp>

#include "ignite/scene/entity.hpp"

namespace ignite
{
    class Scene;
    class IGN_API Physics2D
    {
    public:
        Physics2D();
        ~Physics2D();

        void SimulationStart(Scene *scene);
        void SimulationStop();

        void InstantiateEntity(Entity entity);
        void DestroyEntity(Entity entity);

        void Simulate(float deltaTime);
        void CreateBoxCollider(BoxCollider2DComponent *box, b2BodyId bodyId, b2Vec2 size);
        void CreateCircleCollider(CircleCollider2DComponent *circle, b2BodyId bodyId, float size);

    private:
        Scene *m_Scene;
        b2WorldId m_WorldId{ b2_nullWorldId };
    };
}

#endif
