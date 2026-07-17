/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once
#ifndef IGN_PHYSICS_2D_COMPONENT_HPP
#define IGN_PHYSICS_2D_COMPONENT_HPP

#include "box2d/box2d.h"
#include "box2d/types.h"

#include "ignite/scene/icomponent.hpp"

#include <string>
#include <glm/glm.hpp>

namespace ignite
{
    class Rigidbody2DComponent : public IComponent
    {
    public:
        enum class EBodyType : uint8_t
        {
            Static = 0,
            Kinematic = 1,
            Dynamic = 2
        };

        EBodyType bodyType = EBodyType::Static;

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