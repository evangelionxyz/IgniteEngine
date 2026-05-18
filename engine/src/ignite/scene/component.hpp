// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include "icomponent.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator/animator.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/gpu_data.hpp"
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
    class AnimatorController;
    class ConstantBuffer;
    class ScriptInstance;

    static std::unordered_map<std::string, CompType> s_ComponentsName =
    {
        { "Camera", CompType_Camera },
        { "Arrow", CompType_Arrow },
        { "Rigid Body 2D", CompType_Rigidbody2D },
        { "Directional Light", CompType_DirectionalLight },
        { "Box Collider 2D", CompType_BoxCollider2D },
        { "Circle Collider 2D", CompType_CircleCollider2D },
        { "Sprite 2D", CompType_Sprite2D },
        { "Circle 2D", CompType_Circle2D },
        { "Point Light 2D", CompType_PointLight2D },
        { "Text", CompType_Text },
        { "Mesh", CompType_Mesh },
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
            case CompType_Rigidbody2D: return "CompType_Rigidbody2D";
            case CompType_BoxCollider2D: return "CompType_BoxCollider2D";
            case CompType_CircleCollider2D: return "CompType_CircleCollider2D";
            case CompType_DirectionalLight: return "CompType_CircleCollider2D";
            case CompType_Sprite2D: return "CompType_Sprite2D";
            case CompType_Circle2D: return "CompType_Circle2D";
            case CompType_PointLight2D: return "CompType_PointLight2D";
            case CompType_Mesh: return "CompType_Mesh";
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
        int shadowResolution = 2; // 0=Low, 1=Medium, 2=High, 3=Ultra
        bool cascadeShadow = true;

        COMPONENT_CLASS_TYPE(CompType_DirectionalLight)
    };

    class WorldEnvironment : public IComponent
    {
    public:
        Ref<Environment> environment;
        AssetHandle hdrHandle = AssetHandle(0);

        float exposure = 1.1f;
        float gamma = 2.2f;
        float ambient = 0.5f;

        bool primary = false;
        bool enabled = true;
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

	class MeshComponent : public IComponent
	{
	public:
        AssetHandle handle = AssetHandle(0);         // class SkeletalMesh in mesh.hpp

        glm::mat4 worldMatrix = glm::mat4(1.0f);
        glm::mat4 normalMatrix = glm::mat4(1.0f);

        AABB worldAABB;

        std::string currentStateName;
        float stateElapsed = 0.0f;
        float stateNormalized = 0.0f;
        AssetHandle runtimeAnimatorHandle = AssetHandle(0);
        std::vector<AnimParam> runtimeParams;
        Ref<AnimatorController> runtimeAnimatorInstance = nullptr; // runtime-only for unique animator mode
        Ref<ConstantBuffer> skeletonGpuBuffer = nullptr;
        std::vector<glm::mat4> finalBoneTransforms; // per-entity GPU-ready bone transforms

        // Enable unique for each entity
        bool uniqueAnimator = true;

        MeshComponent() = default;
		COMPONENT_CLASS_TYPE(CompType_Mesh)
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
        glm::vec3 center = { 0.0f, 0.0f, 0.0f };
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

        BoxColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_BoxCollider)
    };

    class PlaneColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        glm::vec3 center = { 0.0f, 0.0f, 0.0f };
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

        PlaneColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_PlaneCollider)
    };

    class SphereColliderComponent: public PhysicsColliderComponent, public IComponent
    {
    public:
        glm::vec3 center = { 0.0f, 0.0f, 0.0f };
        float radius = 1.0f;

        SphereColliderComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_SphereCollider)
    };

    class CapsuleColliderComponent : public PhysicsColliderComponent, public IComponent
    {
    public:
        glm::vec3 center = { 0.0f, 0.0f, 0.0f };
        float radius = 1.0f;
        float height = 2.0f;

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
        ScriptComponent() = default;

        Ref<ScriptInstance> runtimeScriptInstance = nullptr;

        COMPONENT_CLASS_TYPE(CompType_Script)
    };

    // -------------------------------------------------------------------------
    // Animator2DComponent: references an AnimatorController2D asset and holds
    // per-entity runtime playback state so multiple entities can run the same
    // controller independently.
    // -------------------------------------------------------------------------
    class Animator2DComponent : public IComponent
    {
    public:
        AssetHandle controllerHandle = AssetHandle(0);

        // Runtime state (not serialized except currentStateName)
        std::string currentStateName;
        float       stateElapsed    = 0.0f;  // absolute time in current state (seconds)
        float       stateNormalized = 0.0f;  // normalized time [0..1]

        // Per-entity animation runtime (copy of Animation2D playback state)
        int   currentFrame = 0;
        float elapsed      = 0.0f;

        Animator2DComponent() = default;

        COMPONENT_CLASS_TYPE(CompType_Animator2D)
    };

}
