/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
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

#include "ignite/core/types.hpp"
#include "ignite/core/uuid.hpp"

namespace ignite
{
    enum CompType : uint8_t
    {
        CompType_Invalid = 0,
        CompType_ID,
        CompType_Arrow,
        CompType_Transform,
        CompType_Camera,
        CompType_Widget,
        CompType_Sprite2D,
        CompType_Circle2D,
        CompType_PointLight2D,
        CompType_Text,
        CompType_Mesh,
        CompType_DirectionalLight,
        CompType_BoxCollider2D,
        CompType_CircleCollider2D,
        CompType_Rigidbody2D,
        CompType_Rigidbody,
        CompType_BoxCollider,
        CompType_SphereCollider,
        CompType_CapsuleCollider,
        CompType_MeshCollider,
        CompType_AudioSource,
        CompType_Script,
        CompType_WorldEnvironment,
        CompType_Animator2D,
        CompType_LAST
    };

    class IComponent
    {
    public:
        virtual ~IComponent() = default;

        bool dirty = false;

        template<typename T>
        T *As()
        {
            return static_cast<T *>(this);
        }

        UUID GetCompID() const { return m_UUID; }
        virtual CompType GetType() { return CompType_Invalid; };
    private:
        UUID m_UUID;
    };

#define COMPONENT_CLASS_TYPE(Type) \
    static const char *GetName() { return #Type; } \
    static CompType StaticType() { return Type; } \
    virtual CompType GetType() override { return StaticType(); }
}
