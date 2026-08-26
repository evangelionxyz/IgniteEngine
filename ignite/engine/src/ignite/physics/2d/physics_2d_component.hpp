// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PHYSICS_2D_COMPONENT_HPP
#define IGN_PHYSICS_2D_COMPONENT_HPP

#include "box2d/box2d.h"
#include "box2d/types.h"

#include "ignite/scene/icomponent.hpp"
#include "ignite/physics/physics_types.hpp"

#include <string>
#include <glm/glm.hpp>

namespace ignite
{
    class Rigidbody2DComponent : public IComponent
    {
    public:
        physics::BodyType bodyType = physics::BodyType::Static;

        glm::vec2 linearVelocity = { 0.0f, 0.0f };
        float angularVelocity = 0.0f;
        float gravityScale = 1.0f;
        float linearDamping = 0.6f;
        float angularDamping = 0.2f;
        bool isAwake = true;
        bool isEnabled = true;
        bool isEnableSleep = false;
        bool allowFastRotation = true;
        bool fixedRotation = false;
        b2BodyId bodyId = {};
        bool isGizmoDragging = false;

		COMPONENT_CLASS_TYPE(CompType_Rigidbody2D)
    };

    class CircleCollider2DComponent : public IComponent
    {
    public:
        glm::vec2 center{ 0.0f, 0.0f };
        float radius = 0.5f;
		float restitution = 0.1f;
		float friction = 0.5f;
		float density = 1.0f;
        bool isSensor = false;

        b2ShapeId shapeId{};

        COMPONENT_CLASS_TYPE(CompType_CircleCollider2D)
    };

    class BoxCollider2DComponent : public IComponent
    {
    public:
        glm::vec2 size        = {0.5f, 0.5f};
        glm::vec2 offset      = {0.0f, 0.0f};
        float restitution       = 0.1f;
        float friction          = 0.5f;
        float density           = 1.0f;
        bool isSensor         = false;

        b2ShapeId shapeId{};

		COMPONENT_CLASS_TYPE(CompType_BoxCollider2D)
    };
}

#endif
