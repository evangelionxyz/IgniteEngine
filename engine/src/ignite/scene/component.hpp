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

#define GLM_ENABLE_EXPERIMENTAL

#include "icomponent.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/math/aabb.hpp"
#include "scene_camera.hpp"
#include "ignite/core/string_utils.hpp"
#include <string>
#include <glm/glm.hpp>

// Forward declaration
namespace JPH
{
    class Body;
}

namespace ignite
{
    struct MeshPrimitive;
    class MeshInstance;
    class Texture;
    class Skeleton;


    static std::unordered_map<std::string, CompType> s_ComponentsName =
    {
        { "Camera", CompType_Camera },
        { "Rigid Body 2D", CompType_Rigidbody2D },
        { "Box Collider 2D", CompType_BoxCollider2D },
        { "Circle Collider 2D", CompType_CircleCollider2D },
        { "Sprite 2D", CompType_Sprite2D },
        { "Circle 2D", CompType_Circle2D },
        { "Static Mesh", CompType_StaticMesh },
        { "Skeletal Mesh", CompType_SkeletalMesh },
        { "Rigid Body", CompType_Rigidbody },
        { "Box Collider", CompType_BoxCollider },
        { "Sphere Collider", CompType_SphereCollider },
        { "Capsule Collider", CompType_CapsuleCollider },
        { "Mesh Collider", CompType_MeshCollider },
        { "Audio Source", CompType_AudioSource },
        { "C# Script", CompType_Script },
    };

    enum EntityType : uint8_t
    {
        EntityType_Node = BIT(0),
        EntityType_Camera = BIT(1),
        EntityType_Mesh = BIT(2),
        EntityType_Prefab = BIT(3),
        EntityType_Joint = BIT(4),
        EntityType_Audio = BIT(5),
        EntityType_WorldEnvironment = BIT(5),
        EntityType_Invalid = BIT(6)
    };

    // --- 1.  Tiny helpers so the enum behaves like a bit-mask ------------------
    constexpr EntityType operator|(EntityType a, EntityType b)
    {
        return static_cast<EntityType>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr EntityType operator&(EntityType a, EntityType b)
    {
        return static_cast<EntityType>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    // --- 2.  Lookup table: bit value  -> literal string ------------------------
    static constexpr std::array<std::pair<EntityType, const char *>, 7> kEntityNames{ {
        {EntityType_Node  , "Node"},
        {EntityType_Camera, "Camera"},
        {EntityType_Mesh  , "Mesh"},
        {EntityType_Prefab, "Prefab"},
        {EntityType_Joint , "Joint"},
        {EntityType_Audio , "Audio"},
        {EntityType_Invalid,"Invalid"}
    } };

    // --- 3.  Convert an OR-combination of flags to a pipe-separated string ------
    inline std::string EntityTypeFlagsToString(EntityType flags)
    {
        std::ostringstream oss;
        bool first = true;

        for (auto [bit, name] : kEntityNames)
        {
            if ((flags & bit) != static_cast<EntityType>(0))
            {
                if (!first) oss << " | ";
                oss << name;
                first = false;
            }
        }

        if (first) // nothing matched → treat as “Invalid”
            oss << "Invalid";

        return oss.str();
    }

    static EntityType EntityTypeFromStringFlags(const std::string &typeStr)
    {
        static const std::unordered_map<std::string, EntityType> stringToType = {
            {"Node", EntityType_Node},
            {"Camera", EntityType_Camera},
            {"Mesh", EntityType_Mesh},
            {"Prefab", EntityType_Prefab},
            {"Joint", EntityType_Joint},
            {"Audio", EntityType_Audio},
            {"Invalid", EntityType_Invalid}
        };

        EntityType result = static_cast<EntityType>(0);
        std::istringstream stream(typeStr);
        std::string token;

        while (std::getline(stream, token, '|'))
        {
            std::string trimmed = stringutils::Trim(token);
            auto it = stringToType.find(trimmed);
            if (it != stringToType.end())
            {
                result = static_cast<EntityType>(result | it->second);
            }
        }

        return result == static_cast<EntityType>(0) ? EntityType_Invalid : result;
    }

    static const char *ComponentTypeToString(const CompType type)
    {
        switch (type)
        {
            case CompType_Camera: return "CompType_Camera";
            case CompType_Rigidbody2D: return "CompType_Rigidbody2D";
            case CompType_BoxCollider2D: return "CompType_BoxCollider2D";
            case CompType_CircleCollider2D: return "CompType_CircleCollider2D";
            case CompType_Sprite2D: return "CompType_Sprite2D";
            case CompType_Circle2D: return "CompType_Circle2D";
            case CompType_SkeletalMesh: return "CompType_SkeletalMesh";
            case CompType_StaticMesh: return "CompType_StaticMesh";
            case CompType_Rigidbody: return "CompType_Rigidbody";
            case CompType_BoxCollider: return "CompType_BoxCollider";
            case CompType_SphereCollider: return "CompType_SphereCollider";
            case CompType_CapsuleCollider: return "CompType_CapsuleCollider";
            case CompType_MeshCollider: return "CompType_MeshCollider";
            case CompType_AudioSource: return "CompType_AudioSource";
            case CompType_Script: return "CompType_Script";
            case CompType_ID: return "CompType_ID";
            case CompType_Transform: return "CompType_Transform";
            case CompType_Invalid:
            default: return "Invalid Component";
        }
    }

    class IDComponent final : public IComponent
    {
    public:
        std::string name;
        UUID uuid;
        UUID parent = UUID(0);
        std::vector<UUID> children;
        EntityType type;

        void AddChild(UUID childId)
        {
            children.push_back(childId);
        }

        void RemoveChild(UUID childId)
        {
            std::erase_if(children, [childId](const UUID id) 
            { 
                return id == childId;
            });
        }

        bool HasChild() const
        {
            return !children.empty();
        }

        bool IsInType(EntityType type) const
        {
            return (this->type & type) != 0;
        }

        IDComponent(const std::string &_name,  EntityType _type, const UUID &_uuid = UUID())
            : name(_name)
            , type(_type)
            , uuid(_uuid)
        {
        }

        COMPONENT_CLASS_TYPE(CompType_ID)
    };

    class CameraComponent : public IComponent
    {
    public:
        SceneCamera camera;
        bool primary = true;

        CameraComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Camera)
    };

    class TransformComponent : public IComponent
    {
    public:
        // world transforms
        glm::vec3 translation, scale;
        glm::quat rotation;

        // local transforms
        glm::vec3 localTranslation, localScale;
        glm::quat localRotation;

        bool isAnimated = false;
        bool visible = true;

        TransformComponent() = default;

        TransformComponent(const glm::vec3 &_translation)
            : translation(_translation)
            , rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
            , scale(glm::vec3(1.0f))
            , localTranslation(_translation)
            , localRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
            , localScale(glm::vec3(1.0f))
        {
        }

        TransformComponent(const glm::vec3 &_translation, const glm::quat &_rotation, const glm::vec3 _scale)
            : translation(_translation)
            , rotation(_rotation)
            , scale(_scale)
            , localTranslation(_translation)
            , localRotation(_rotation)
            , localScale(_scale)
        {
        }

        // local transformation
        void SetLocalTranslation(const glm::vec3 &newTranslation)
        {
            localTranslation = newTranslation;
            dirty = true;
        }

        void SetLocalRotation(const glm::quat &newRotation)
        {
            localRotation = newRotation;
            dirty = true;
        }

        void SetLocalScale(const glm::vec3 &newScale)
        {
            localScale = newScale;
            dirty = true;
        }

        glm::mat4 GetLocalMatrix() const
        {
            return glm::translate(glm::mat4(1.0f), localTranslation) * glm::mat4(localRotation) * glm::scale(glm::mat4(1.0f), localScale);
        }

        // World transformation
        void SetWorldTranslation(const glm::vec3 &newTranslation)
        {
            translation = newTranslation;
            dirty = true;
        }

        void SetWorldRotation(const glm::quat &newRotation)
        {
            rotation = newRotation;
            dirty = true;
        }

        void SetWorldScale(const glm::vec3 &newScale)
        {
            scale = newScale;
            dirty = true;
        }

        glm::mat4 GetWorldMatrix() const
        {
            return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }

        COMPONENT_CLASS_TYPE(CompType_Transform)
    };

    class WorldEnvironment : public IComponent
    {
    public:
        Ref<Environment> environment;
        AssetHandle hdrHandle;

        bool primary = false;

        COMPONENT_CLASS_TYPE(CompType_WorldEnvironment)
    };

    class Sprite2DComponent : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0); // Texture handle
        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec2 tilingFactor = { 1.0f, 1.0f };

        COMPONENT_CLASS_TYPE(CompType_Sprite2D)
    };

	class Circle2DComponent : public IComponent
	{
	public:
		glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float thickness = 1.0f;
        float fade = 0.005f;

		COMPONENT_CLASS_TYPE(CompType_Circle2D)
	};

    class StaticMeshComponent : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0);
        Ref<ConstantBuffer> perEntityBuffer;
        nvrhi::BindingSetHandle meshBindingSet = nullptr; // Cached binding set - reused across frames

        StaticMeshComponent() = default;

		COMPONENT_CLASS_TYPE(CompType_StaticMesh)
    };

	class SkeletalMeshComponent : public IComponent
	{
	public:
        SkeletalMeshComponent() = default;
		COMPONENT_CLASS_TYPE(CompType_SkeletalMesh)
	};

    class RigibodyComponent : public IComponent
    {
    public:
        enum class EMotionQuality
        {
            Discrete = 0,
            LinearCast = 1
        };

        EMotionQuality MotionQuality = EMotionQuality::Discrete;

        bool useGravity = true;
        bool rotateX = true, rotateY = true, rotateZ = true;
        bool moveX = true, moveY = true, moveZ = true;
        bool isStatic = false;
        float mass = 1.0f;
        bool allowSleeping = true;
        bool retainAcceleration = false;
        float gravityFactor = 1.0f;
        glm::vec3 centerMass = { 0.0f, 0.0f, 0.0f };

        JPH::Body *body = nullptr;

        RigibodyComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Rigidbody)
    };

    class PhysicsColliderComponent
    {
    public:
        float friction = 0.6f;
        float staticFriction = 0.6f;
        float restitution = 0.6f;
        float density = 1.0f;

        void *shape = nullptr;
    };

    class BoxColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

        BoxColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_BoxCollider)
    };

    class SphereColliderComponent: public PhysicsColliderComponent, public IComponent
    {
    public:
        float radius = 0.5f;

        SphereColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_SphereCollider)
    };

    class CapsuleColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        float radius = 0.5f;
        float height = 1.0f;

        CapsuleColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_CapsuleCollider)
    };

    class MeshColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        bool convex = false; // Whether to use convex hull or triangle mesh

        MeshColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_MeshCollider)
    };

    class AudioSourceComponent : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0);

        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        bool playOnStart = false;

        AudioSourceComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_AudioSource)
    };

    class ScriptComponent : public IComponent
    {
    public:
        std::string className = "EMPTY";
        ScriptComponent() = default;


        COMPONENT_CLASS_TYPE(CompType_Script)
    };

}
