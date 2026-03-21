/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
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
#include "box2d/box2d.h"
#include "box2d/types.h"

#include "ignite/scene/icomponent.hpp"

#include <string>
#include <glm/glm.hpp>

namespace ignite
{
    enum Body2DType
    {
        Body2DType_Static = 0, Body2DType_Dynamic, Body2DType_Kinematic
    };

    static b2BodyType GetB2BodyType(Body2DType type)
    {
        switch (type)
        {
        case Body2DType_Static: return b2_staticBody;
        case Body2DType_Dynamic: return b2_dynamicBody;
        case Body2DType_Kinematic: return b2_kinematicBody;
        }
        return b2_staticBody;
    }

    static std::string BodyTypeToString(Body2DType type)
    {
        switch (type)
        {
            case ignite::Body2DType_Static: return "Static";
            case ignite::Body2DType_Dynamic: return "Dynamic";
            case ignite::Body2DType_Kinematic: return "Kinematic";
            default: return "Invalid";
        }
    }

    static Body2DType BodyTypeFromString(const std::string &typeStr)
    {
        if (typeStr == "Static") return Body2DType_Static;
        else if (typeStr == "Dynamic") return Body2DType_Dynamic;
        else if (typeStr == "Kinematic") return Body2DType_Kinematic;
        return Body2DType_Static;
    }

    class Rigidbody2DComponent : public IComponent
    {
    public:
        Body2DType type = Body2DType_Static;
        glm::vec2 linearVelocity = { 0.0f, 0.0f };
        f32 angularVelocity = 0.0f;
        f32 gravityScale = 1.0f;
        f32 linearDamping = 0.6f;
        f32 angularDamping = 0.2f;
        bool isAwake = true;
        bool isEnabled = true;
        bool isEnableSleep = false;
        bool allowFastRotation = true;
        bool fixedRotation = false;
        b2BodyId bodyId = {};

        static CompType StaticType() { return CompType_Rigidbody2D; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class BoxCollider2DComponent : public IComponent
    {
    public:
        glm::vec2 size        = {0.5f, 0.5f};
        glm::vec2 offset      = {0.0f, 0.0f};
        glm::vec2 currentSize = {0.5f, 0.5f};
        f32 restitution       = 0.1f;
        f32 friction          = 0.5f;
        f32 density           = 1.0f;
        bool isSensor         = false;

        b2ShapeId shapeId{};

        static CompType StaticType() { return CompType_BoxCollider2D; }
        virtual CompType GetType() override { return StaticType(); }
    };
}
