// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_COMPONENT_HPP
#define IGN_COMPONENT_HPP

#ifdef uuid
#undef uuid
#endif

#include "icomponent.hpp"
#include "ignite/terrain/terrain.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator/animator.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/procedural_sky.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/math/transform.hpp"
#include "scene_camera.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/physics/3d/physics_3d.hpp"

#include <string>
#include <glm/glm.hpp>

// Forward declaration
namespace JPH
{
    class Body;
    class CharacterVirtual;
}

namespace ignite
{
    class Texture;
    class Skeleton;
    class AnimatorController;
    class ConstantBuffer;
    class ScriptInstance;

    static std::unordered_map<std::string, CompType> s_ComponentsName =
    {
        { "Camera", CompType_Camera },
        { "Arrow", CompType_Arrow },
        { "Rendering", CompType_Rendering },
        { "Rigid Body 2D", CompType_Rigidbody2D },
        { "Directional Light", CompType_DirectionalLight },
        { "Box Collider 2D", CompType_BoxCollider2D },
        { "Circle Collider 2D", CompType_CircleCollider2D },
        { "Sprite 2D", CompType_Sprite2D },
        { "Circle 2D", CompType_Circle2D },
        { "Point Light 2D", CompType_PointLight2D },
        { "Text", CompType_Text },
        { "SkeletalMesh", CompType_SkeletalMesh },
        { "StaticMesh", CompType_StaticMesh },
        { "StaticMesh", CompType_StaticMesh },
        { "Rigid Body", CompType_Rigidbody },
        { "Box Collider", CompType_BoxCollider },
        { "Widget", CompType_Widget },
        { "Sphere Collider", CompType_SphereCollider },
        { "Capsule Collider", CompType_CapsuleCollider },
        { "Mesh Collider", CompType_MeshCollider },
        { "Audio Source", CompType_AudioSource },
        { "World Environment", CompType_WorldEnvironment },
        { "C# Script", CompType_Script },
        { "Animator 2D", CompType_Animator2D },
        { "Point Light", CompType_PointLight },
        { "Spot Light", CompType_SpotLight },
        { "Character Controller", CompType_CharacterController },
        { "Terrain", CompType_Terrain },
        { "HeightField Collider", CompType_HeightFieldCollider },
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
            case CompType_Arrow: return "CompType_Arrow";
            case CompType_Rendering: return "CompType_Rendering";
            case CompType_Rigidbody2D: return "CompType_Rigidbody2D";
            case CompType_BoxCollider2D: return "CompType_BoxCollider2D";
            case CompType_CircleCollider2D: return "CompType_CircleCollider2D";
            case CompType_DirectionalLight: return "CompType_CircleCollider2D";
            case CompType_Sprite2D: return "CompType_Sprite2D";
            case CompType_Circle2D: return "CompType_Circle2D";
            case CompType_PointLight2D: return "CompType_PointLight2D";
            case CompType_SkeletalMesh: return "CompType_SkeletalMesh";
            case CompType_StaticMesh: return "CompType_StaticMesh";
            case CompType_Rigidbody: return "CompType_Rigidbody";
            case CompType_PlaneCollider: return "CompType_PlaneCollider";
            case CompType_BoxCollider: return "CompType_BoxCollider";
            case CompType_SphereCollider: return "CompType_SphereCollider";
            case CompType_CapsuleCollider: return "CompType_CapsuleCollider";
            case CompType_MeshCollider: return "CompType_MeshCollider";
            case CompType_AudioSource: return "CompType_AudioSource";
            case CompType_Text: return "CompType_Text";
            case CompType_Script: return "CompType_Script";
            case CompType_ID: return "CompType_ID";
            case CompType_Transform: return "CompType_Transform";
            case CompType_Widget: return "CompType_Widget";
            case CompType_PointLight: return "CompType_PointLight";
            case CompType_SpotLight: return "CompType_SpotLight";
            case CompType_CharacterController: return "CompType_CharacterController";
            case CompType_Terrain: return "CompType_Terrain";
            case CompType_Invalid:
            default: return "Invalid Component";
        }
    }

#ifdef uuid
#undef uuid
#endif
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

    class ArrowComponent : public IComponent
    {
    public:
        AssetHandle placeHolder = AssetHandle(0);

        ArrowComponent() = default;
        COMPONENT_CLASS_TYPE(CompType_Arrow)
    };

    class TransformComponent : public IComponent
    {
    public:
        Transform world;
        Transform local;

        TransformComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Transform)
    };

    class RenderingComponent : public IComponent
    {
    public:
        bool visible = true;

        RenderingComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Rendering)
    };

    class DirectionalLightComponent : public IComponent
    {
    public:
        DirectionalLightComponent() = default;

        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float intensity = 0.5f;
        float angularRadius = 45.0f; // degrees

        // Shadow
        float shadowStrength = 0.5f;
        float shadowMinBias = 0.05f;
        float shadowMaxBias = 0.001f;
        float pcfRadius = 0.3f;
        float shadowDistance = 200.0f; // max distance (world units) from camera at which shadows are rendered
        int shadowResolution = 2; // 0=Low, 1=Medium, 2=High, 3=Ultra
        bool cascadeShadow = true;

        COMPONENT_CLASS_TYPE(CompType_DirectionalLight)
    };

    class PointLightComponent : public IComponent
    {
    public:
        PointLightComponent() = default;

        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float range = 10.0f;
        bool enabled = true;

        // Attenuation: 1 / (constant + linear*d + quadratic*d^2)
        float constantAttenuation = 1.0f;
        float linearAttenuation = 0.09f;
        float quadraticAttenuation = 0.032f;

        COMPONENT_CLASS_TYPE(CompType_PointLight)
    };

    class SpotLightComponent : public IComponent
    {
    public:
        SpotLightComponent() = default;

        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float range = 10.0f;
        bool enabled = true;

        // Attenuation: 1 / (constant + linear*d + quadratic*d^2)
        float constantAttenuation = 1.0f;
        float linearAttenuation = 0.09f;
        float quadraticAttenuation = 0.032f;

        // Cone angles in degrees
        float innerConeAngle = 12.5f;  // Full-intensity inner cone
        float outerConeAngle = 45.0f;  // Falloff outer cone

        COMPONENT_CLASS_TYPE(CompType_SpotLight)
    };

    class WorldEnvironment : public IComponent
    {
    public:
        Ref<Environment> environment;
        AssetHandle hdrHandle = AssetHandle(0);

        SkyType skyType = SkyType::HDRI;
        AtmosphereParams atmosphereParams;

        float exposure = 1.1f;
        float gamma = 2.2f;
        float ambient = 0.5f;

        // Fog
        float fogDensity = 0.0f;
        glm::vec4 fogColor = glm::vec4(0.5f, 0.6f, 0.7f, 1.0f);
        float fogStart = 10.0f;
        float fogEnd = 100.0f;

        bool gpuInitialized = false;
        bool dirtyEnvironment = true;

        COMPONENT_CLASS_TYPE(CompType_WorldEnvironment)
    };

    class Sprite2DComponent : public IComponent
    {
    public:
        AssetHandle handle         = AssetHandle(0); // Texture handle
        AssetHandle materialHandle = AssetHandle(0); // Material2D handle

        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec2 tilingFactor = { 1.0f, 1.0f };

        glm::vec2 uv0 = { 0.0f, 1.0f };
        glm::vec2 uv1 = { 1.0f, 0.0f };

        bool flipY = false;
        bool flipX = false;
        
        COMPONENT_CLASS_TYPE(CompType_Sprite2D)
    };

    class PointLight2DComponent : public IComponent
    {
    public:
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float radius = 5.0f;
        float intensity = 1.0f;
        bool enabled = true;

        COMPONENT_CLASS_TYPE(CompType_PointLight2D)
    };

    class Circle2DComponent : public IComponent
    {
    public:
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float thickness = 1.0f;
        float fade = 0.005f;

        COMPONENT_CLASS_TYPE(CompType_Circle2D)
    };

    class TextComponent : public IComponent
    {
    public:
        AssetHandle fontHandle = AssetHandle(0);
        AssetHandle material2dHandle = AssetHandle(0);

        std::string text = "Empty";

        glm::vec4 color = glm::vec4(1.0f);
        float kerning = 0.0f;
        float lineSpacing = -0.025f;
        bool screenSpace = false;

        TextComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Text)
    };

    class WidgetComponent : public IComponent
    {
    public:
        AssetHandle widgetHandle = AssetHandle(0);

        WidgetComponent() = default;
        COMPONENT_CLASS_TYPE(CompType_Widget)
    };

    class StaticMeshComponent : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0);                    // class Mesh in mesh.hpp
        std::unordered_map<int, AssetHandle> overrideMaterials; // Mesh index, Material Handle
        glm::mat4 normalMatrix = glm::mat4(1.0f);
        AABB worldAABB;

        // ==== RUNTIME DATA ====
        std::vector<Mesh_GPUData> cachedInstanceTransforms; // cached transforms per sub-mesh instance

        StaticMeshComponent() = default;
        COMPONENT_CLASS_TYPE(CompType_StaticMesh)
    };

    class SkeletalMeshComponent : public IComponent
    {
    public:
        AssetHandle handle = AssetHandle(0);                    // class SkeletalMesh in mesh.hpp
        std::unordered_map<int, AssetHandle> overrideMaterials; // Mesh index, Material Handle

        glm::mat4 normalMatrix = glm::mat4(1.0f);
        AABB worldAABB;

        // ==== RUNTIME DATA ====
        std::string currentStateName;
        float stateElapsed = 0.0f;
        float stateNormalized = 0.0f;
        glm::vec2 blendSpaceSmoothedInput = glm::vec2(0.0f);
        glm::vec2 blendSpaceVelocity = glm::vec2(0.0f);
        AssetHandle runtimeAnimatorHandle = AssetHandle(0);
        std::unordered_map<std::string, AnimParam> runtimeParams;
        Ref<AnimatorController> runtimeAnimatorInstance = nullptr; // runtime-only for unique animator mode
        std::vector<glm::mat4> finalBoneTransforms; // per-entity GPU-ready bone transforms
        std::vector<Mesh_GPUData> cachedInstanceTransforms; // cached transforms per sub-mesh instance

        // ==== Socket System ====
        // Cache of animated joint transforms in model space (before inverse bind pose multiplication)
        std::vector<glm::mat4> globalJointTransforms;

        // Mapping from socket name to attached Mesh asset handle
        std::unordered_map<std::string, AssetHandle> socketAttachments;

        // Enable unique for each entity
        bool uniqueAnimator = true;

        void ResetAnimatorRuntime()
        {
			// Reset runtime
			this->currentStateName.clear();
			this->stateElapsed = 0.0f;
			this->stateNormalized = 0.0f;
			this->blendSpaceSmoothedInput = glm::vec2(0.0f);
			this->blendSpaceVelocity = glm::vec2(0.0f);
        }

        glm::mat4 GetSocketWorldTransform(const glm::mat4 &meshWorldMatrix, const Skeleton &skeleton, const std::string &socketName) const
        {
            const auto it = skeleton.socketNameToIndex.find(socketName);
            if (it == skeleton.socketNameToIndex.end())
                return meshWorldMatrix;

            const int32_t socketIndex = it->second;
            if (socketIndex < 0 || socketIndex >= static_cast<int32_t>(skeleton.sockets.size()))
                return meshWorldMatrix;

            const JointSocket &socket = skeleton.sockets[socketIndex];
            glm::mat4 socketLocal = socket.local.GetMatrix();

            // If active bone global transforms exist from animation update, use them
            if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(this->globalJointTransforms.size()))
            {
                return meshWorldMatrix * this->globalJointTransforms[socket.parentJointId] * socketLocal;
            }

            // Fallback: use default bind pose global transforms from skeleton
            if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton.joints.size()))
            {
                return meshWorldMatrix * skeleton.joints[socket.parentJointId].globalTransform * socketLocal;
            }

            return meshWorldMatrix * socketLocal;
        }

        SkeletalMeshComponent() = default;
        COMPONENT_CLASS_TYPE(CompType_SkeletalMesh)
    };

    class RigidbodyComponent : public IComponent
    {
    public:
        using CollisionGroup = uint32_t;

        physics::MotionQuality motionQuality = physics::MotionQuality::Discrete;
        physics::BodyType bodyType = physics::BodyType::Static;

        bool useGravity = true;
        bool rotateX = true, rotateY = true, rotateZ = true;
        bool moveX = true, moveY = true, moveZ = true;
        bool allowSleeping = true;
        bool isSensor = false;
        bool retainAcceleration = false;
		bool applyGyroscopicForce = false;

        float mass = 1.0f;
        float gravityFactor = 1.0f;
		float linearDamping = 0.0f;
		float angularDamping = 0.05f;
		float friction = 0.2f;
		float restitution = 0.0f;
		float maxLinearVelocity = 500.0f; // Jolt's default
		float maxAngularVelocity = 0.25f * 3.14159265358979323846f * 60.0f; // Jolt's default - JPH_PI

        glm::vec3 angularVelocity = { 0.0f, 0.0f, 0.0f };
        glm::vec3 linearVelocity = { 0.0f, 0.0f, 0.0f };
        glm::vec3 centerMass = { 0.0f, 0.0f, 0.0f };

        WeakRef<physics::PhysicsDynamicActor> dynamicActor;
        WeakRef<physics::PhysicsStaticActor> staticActor;
        bool isGizmoDragging = false;

        RigidbodyComponent() = default;
        RigidbodyComponent(const RigidbodyComponent &other)
            : motionQuality(other.motionQuality)
            , bodyType(other.bodyType)
            , useGravity(other.useGravity)
            , rotateX(other.rotateX), rotateY(other.rotateY), rotateZ(other.rotateZ)
            , moveX(other.moveX), moveY(other.moveY), moveZ(other.moveZ)
            , allowSleeping(other.allowSleeping)
            , isSensor(other.isSensor)
            , retainAcceleration(other.retainAcceleration)
            , applyGyroscopicForce(other.applyGyroscopicForce)
            , mass(other.mass)
            , gravityFactor(other.gravityFactor)
            , linearDamping(other.linearDamping)
            , angularDamping(other.angularDamping)
            , friction(other.friction)
            , restitution(other.restitution)
            , maxLinearVelocity(other.maxLinearVelocity)
            , maxAngularVelocity(other.maxAngularVelocity)
            , angularVelocity(other.angularVelocity)
            , linearVelocity(other.linearVelocity)
            , centerMass(other.centerMass)
            , dynamicActor(other.dynamicActor)
            , staticActor(other.staticActor)
            , isGizmoDragging(other.isGizmoDragging)
        {
        }

        RigidbodyComponent &operator=(const RigidbodyComponent &other)
        {
            if (this != &other)
            {
                motionQuality = other.motionQuality;
                bodyType = other.bodyType;
                useGravity = other.useGravity;
                rotateX = other.rotateX; rotateY = other.rotateY; rotateZ = other.rotateZ;
                moveX = other.moveX; moveY = other.moveY; moveZ = other.moveZ;
                allowSleeping = other.allowSleeping;
                isSensor = other.isSensor;
                retainAcceleration = other.retainAcceleration;
                applyGyroscopicForce = other.applyGyroscopicForce;
                mass = other.mass;
                gravityFactor = other.gravityFactor;
                linearDamping = other.linearDamping;
                angularDamping = other.angularDamping;
                friction = other.friction;
                restitution = other.restitution;
                maxLinearVelocity = other.maxLinearVelocity;
                maxAngularVelocity = other.maxAngularVelocity;
                angularVelocity = other.angularVelocity;
                linearVelocity = other.linearVelocity;
                centerMass = other.centerMass;
                dynamicActor = other.dynamicActor;
                staticActor = other.staticActor;
                isGizmoDragging = other.isGizmoDragging;
            }
            return *this;
        }

        COMPONENT_CLASS_TYPE(CompType_Rigidbody)
    };

    class PhysicsColliderComponent
    {
    public:
		glm::vec3 center = { 0.0f, 0.0f, 0.0f };
        WeakRef<physics::PhysicsCollider> collider;

        PhysicsColliderComponent() = default;
        PhysicsColliderComponent(const PhysicsColliderComponent &other)
            : center(other.center)
            , collider(other.collider)
        {
        }

        PhysicsColliderComponent &operator=(const PhysicsColliderComponent &other)
        {
            if (this != &other)
            {
                center = other.center;
                collider = other.collider;
            }
            return *this;
        }
    };

    class BoxColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

        BoxColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_BoxCollider)
    };

    class PlaneColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

        PlaneColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_PlaneCollider)
    };

    class SphereColliderComponent: public PhysicsColliderComponent, public IComponent
    {
    public:
        float radius = 1.0f;

        SphereColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_SphereCollider)
    };

    class CapsuleColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        float radius = 0.5f;
        float height = 2.0f;

        CapsuleColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_CapsuleCollider)
    };

    class CharacterControllerComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        float radius = 0.5f;
        float height = 2.0f;
        float maxStepHeight = 0.4f;
        float maxSlopeAngle = 45.0f; // in degrees
        float mass = 80.0f;
        float friction = 0.2f;
        float gravityFactor = 1.0f;

        glm::vec3 up = { 0.0f, 1.0f, 0.0f };
        glm::vec3 linearVelocity = { 0.0f, 0.0f, 0.0f };

        WeakRef<physics::PhysicsCharacterController> character;

        CharacterControllerComponent() = default;
        CharacterControllerComponent(const CharacterControllerComponent &other)
            : PhysicsColliderComponent(other)
            , radius(other.radius)
            , height(other.height)
            , maxStepHeight(other.maxStepHeight)
            , maxSlopeAngle(other.maxSlopeAngle)
            , mass(other.mass)
            , friction(other.friction)
            , gravityFactor(other.gravityFactor)
            , up(other.up)
            , linearVelocity(other.linearVelocity)
            , character(other.character)
        {
        }

        CharacterControllerComponent &operator=(const CharacterControllerComponent &other)
        {
            if (this != &other)
            {
                PhysicsColliderComponent::operator=(other);
                radius = other.radius;
                height = other.height;
                maxStepHeight = other.maxStepHeight;
                maxSlopeAngle = other.maxSlopeAngle;
                mass = other.mass;
                friction = other.friction;
                gravityFactor = other.gravityFactor;
                up = other.up;
                linearVelocity = other.linearVelocity;
                character = other.character;
            }
            return *this;
        }

        COMPONENT_CLASS_TYPE(CompType_CharacterController)
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

    class HeightFieldColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        uint32_t sampleCount = 0;
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
        std::vector<float> heights;

        HeightFieldColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_HeightFieldCollider)
    };

    class AudioSourceComponent : public IComponent
    {
    public:
        enum class DspType : uint8_t
        {
            Reverb = 0,
            Distortion,
            Chorus,
            Compressor,
            Delay,
        };

        struct DspSettings
        {
            DspType type = DspType::Reverb;
            bool enabled = true;

            float reverbDecayTime = 1500.0f;
            float reverbEarlyDelay = 20.0f;
            float reverbLateDelay = 40.0f;
            float reverbHighFrequencyReference = 5000.0f;
            float reverbDiffusion = 50.0f;
            float reverbDensity = 50.0f;
            float reverbLowShelfGain = 250.0f;
            float reverbHighCut = 20000.0f;
            float reverbDryLevel = 0.0f;
            float reverbWetLevel = -6.0f;

            float distortionLevel = 0.5f;

            float chorusMix = 50.0f;
            float chorusRate = 0.8f;
            float chorusDepth = 3.0f;

            float compressorThreshold = 0.0f;
            float compressorRatio = 2.5f;
            float compressorRelease = 100.0f;
            float compressorGainMakeup = 0.0f;
            bool compressorUseSidechain = false;

            float delayMs = 250.0f;
            float delayFeedback = 20.0f;
        };

        AssetHandle handle = AssetHandle(0);

        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        bool playOnStart = false;
        bool loop = false;

        std::vector<DspSettings> dsps;

        AudioSourceComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_AudioSource)
    };

    class ScriptComponent : public IComponent
    {
    public:
        std::string className = "EMPTY";
        Ref<ScriptInstance> runtimeScriptInstance = nullptr;

        ScriptComponent() = default;
        COMPONENT_CLASS_TYPE(CompType_Script)
    };

    class TerrainComponent : public IComponent
    {
    public:
        uint32_t resolution = 64;
        float worldSize = 100.0f;
        float maxHeight = 50.0f;
        float noiseFrequency = 0.6f;
        int noiseSeed = 1337;
        uint32_t chunkCount = 4;
        uint32_t lodLevels = 3;

        // TODO: heightmap handle should takes image from ContentBrowser
        AssetHandle heightmapHandle = AssetHandle(0);

        Ref<TerrainData> data = nullptr;
        std::vector<TerrainChunk> chunks;
        bool gpuInitialized = false;
        WeakRef<physics::PhysicsCollider> collider;

        TerrainComponent() = default;
        ~TerrainComponent()
        {
            ReleaseGPU();
        }

        void ReleaseGPU()
        {
            for (auto &chunk : chunks)
            {
                chunk.primitive.reset();
            }
            chunks.clear();
            gpuInitialized = false;
        }

        COMPONENT_CLASS_TYPE(CompType_Terrain)
    };

    class Animator2DComponent : public IComponent
    {
    public:
        AssetHandle controllerHandle = AssetHandle(0);

        // Runtime state (not serialized except currentStateName)
        std::string currentStateName;
        float stateElapsed = 0.0f;  // absolute time in current state (seconds)
        float stateNormalized = 0.0f;  // normalized time [0..1]

        // Per-entity animation runtime (copy of Animation2D playback state)
        int   currentFrame = 0;
        float elapsed = 0.0f;

        Animator2DComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Animator2D)
    };

}

#endif
