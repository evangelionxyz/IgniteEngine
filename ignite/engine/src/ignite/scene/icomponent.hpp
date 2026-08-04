// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ICOMPONENT_HPP
#define IGN_ICOMPONENT_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/uuid.hpp"

namespace ignite
{
    enum CompType : uint8_t
    {
        CompType_Invalid = 0,
        CompType_ID,
        CompType_Rendering,
        CompType_Arrow,
        CompType_Transform,
        CompType_Camera,
        CompType_Widget,
        CompType_Sprite2D,
        CompType_Circle2D,
        CompType_PointLight2D,
        CompType_Text,
        CompType_SkeletalMesh,
        CompType_StaticMesh,
        CompType_DirectionalLight,
        CompType_BoxCollider2D,
        CompType_CircleCollider2D,
        CompType_Rigidbody2D,
        CompType_Rigidbody,
        CompType_PlaneCollider,
        CompType_BoxCollider,
        CompType_SphereCollider,
        CompType_CapsuleCollider,
        CompType_MeshCollider,
        CompType_AudioSource,
        CompType_Script,
        CompType_WorldEnvironment,
        CompType_Animator2D,
        CompType_PointLight,
        CompType_SpotLight,
        CompType_CharacterController,
        CompType_Terrain,

        CompType_COUNT
    };

    class IGN_API IComponent
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

#endif
