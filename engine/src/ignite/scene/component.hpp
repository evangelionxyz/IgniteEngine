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
#include "ignite/graphics/material.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/mesh.hpp"
#include "ignite/graphics/environment.hpp"
#include "ignite/math/aabb.hpp"
#include "scene_camera.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <nvrhi/nvrhi.h>
#include <string>

#include "ignite/core/string_utils.hpp"

// Forward declaration
namespace JPH
{
    class Body;
}

namespace ignite
{
    class Texture;
    class Mesh;
    class Skeleton;

    static std::unordered_map<std::string, CompType> s_ComponentsName =
    {
        { "Camera", CompType_Camera },
        { "Rigid Body 2D", CompType_Rigidbody2D },
        { "Box Collider 2D", CompType_BoxCollider2D },
        { "Sprite 2D", CompType_Sprite2D},
        { "Skeletal Mesh", CompType_SkeletalMesh},
        { "Rigid Body", CompType_Rigidbody},
        { "Box Collider", CompType_BoxCollider},
        { "Sphere Collider", CompType_SphereCollider},
        { "Capsule Collider", CompType_CapsuleCollider},
        { "Mesh Collider", CompType_MeshCollider},
        { "Audio Source", CompType_AudioSource},
        { "C# Script", CompType_Script},
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
            case CompType_Sprite2D: return "CompType_Sprite2D";
            case CompType_SkeletalMesh: return "CompType_SkeletalMesh";
            case CompType_Rigidbody: return "CompType_Rigidbody";
            case CompType_BoxCollider: return "CompType_BoxCollider";
            case CompType_SphereCollider: return "CompType_SphereCollider";
            case CompType_CapsuleCollider: return "CompType_CapsuleCollider";
            case CompType_MeshCollider: return "CompType_MeshCollider";
            case CompType_AudioSource: return "CompType_AudioSource";
            case CompType_Script: return "CompType_Script";
            case CompType_ID: return "CompType_ID";
            case CompType_Transform: return "CompType_Transform";
            case CompType_StaticMesh: return "CompType_StaticMesh";
            case CompType_Invalid:
            default: return "Invalid Component";
        }
    }

    class ID final : public IComponent
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

        ID(const std::string &_name,  EntityType _type, const UUID &_uuid = UUID())
            : name(_name)
            , type(_type)
            , uuid(_uuid)
        {
        }

        static CompType StaticType() { return CompType_ID; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class Camera : public IComponent
    {
    public:
        SceneCamera camera;
        bool primary = true;

        Camera() = default;

        static CompType StaticType() { return CompType_Camera; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class Transform : public IComponent
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

        Transform() = default;

        Transform(const glm::vec3 &_translation)
            : translation(_translation)
            , rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
            , scale(glm::vec3(1.0f))
            , localTranslation(_translation)
            , localRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
            , localScale(glm::vec3(1.0f))
        {
        }

        Transform(const glm::vec3 &_translation, const glm::quat &_rotation, const glm::vec3 _scale)
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

        static CompType StaticType() { return CompType_Transform; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class WorldEnvironment : public IComponent
    {
    public:
        Ref<Environment> environment;
        EnvironmentParams params;
        AssetHandle imageHandle;

        bool primary = false;
        
        static CompType StaticType() { return CompType_WorldEnvironment; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class Sprite2D : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0); // Texture handle
        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec2 tilingFactor = { 1.0f, 1.0f };

        static CompType StaticType() { return CompType_Sprite2D; }
        virtual CompType GetType() override { return StaticType(); }
    };

     class SkeletalMesh : public IComponent
     {
     public:
         AssetHandle meshHandle = AssetHandle(0); // Primitive Mesh data
         AssetHandle skeletonHandle = AssetHandle(0);
         AssetHandle activeAnimationHandle = AssetHandle(0);

         std::vector<AssetHandle> animationHandle;
         std::vector<glm::mat4> boneTransforms;

         // for rendering
         std::vector<Ref<MeshInstance>> meshes;

         SkeletalMesh() = default;

         static CompType StaticType() { return CompType_SkeletalMesh; }
         virtual CompType GetType() override { return StaticType(); }
     };

    class StaticMesh : public IComponent
    {
    public:
        AssetHandle meshHandle = AssetHandle(0);

        StaticMesh() = default;

        static CompType StaticType() { return CompType_StaticMesh; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class Rigibody : public IComponent
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

        Rigibody() = default;

        static CompType StaticType() { return CompType_Rigidbody; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class PhysicsCollider
    {
    public:
        float friction = 0.6f;
        float staticFriction = 0.6f;
        float restitution = 0.6f;
        float density = 1.0f;

        void *shape = nullptr;
    };

    class BoxCollider : public PhysicsCollider, public IComponent
    {
    public:
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

        BoxCollider() = default;

        static CompType StaticType() { return CompType_BoxCollider; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class SphereCollider: public PhysicsCollider, public IComponent
    {
    public:
        float radius = 0.5f;

        SphereCollider() = default;

        static CompType StaticType() { return CompType_SphereCollider; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class CapsuleCollider : public PhysicsCollider, public IComponent
    {
    public:
        float radius = 0.5f;
        float height = 1.0f;

        CapsuleCollider() = default;

        static CompType StaticType() { return CompType_CapsuleCollider; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class MeshCollider : public PhysicsCollider, public IComponent
    {
    public:
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        bool convex = false; // Whether to use convex hull or triangle mesh

        MeshCollider() = default;

        static CompType StaticType() { return CompType_MeshCollider; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class AudioSource : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0);

        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        bool playOnStart = false;

        AudioSource() = default;

        static CompType StaticType() { return CompType_AudioSource; }
        virtual CompType GetType() override { return StaticType(); }
    };

    class Script : public IComponent
    {
    public:
        std::string className = "EMPTY";
        Script() = default;

        static CompType StaticType() { return CompType_Script; }
        virtual CompType GetType() override { return StaticType(); }
    };
}
