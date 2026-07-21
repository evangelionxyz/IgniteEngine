// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_HPP
#define IGN_ASSET_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/path.hpp"

#include <string>
#include <map>
#include <functional>

namespace ignite
{
    class Serializer;
    class Project;

    using AssetHandle = UUID;

    enum class AssetType
    {
        Invalid,
        Metadata,

        Auto,
        Audio,
        ScriptableObject,
        AudioMixer,
        SoundCue,
        Model,
        Project,
        Texture,
        SpriteSheet,
        Shader,
        Material,
        Font,
        TextureCube,
        SkeletalAnimation,
        AnimationMontage,
        BlendSpace,
        LocomotionController,
        Environment,
        Anim2D,
        Skeleton,
        Mesh, // .mesh (.fbx, .gltf, .glb)
        StaticMesh, // .mesh - engine specific
        SkeletalMesh, // .skmesh - engine specific
		InputMapping, // .ixinput - engine specific
        Scene,
        Widget,
        AnimatorController, // .ac    - animator state machine

        Material2D,

        Animation2D,          // .anim2d  - single 2D animation clip
        AnimatorController2D, // .ac2d    - 2D animator state machine
    };

    static inline std::string AssetTypeToString(AssetType type)
    {
        switch (type)
        {
            case ignite::AssetType::Texture: return "Texture";
            case ignite::AssetType::Widget: return "Widget";
            case ignite::AssetType::Shader: return "Shader";
            case ignite::AssetType::AnimationMontage: return "AnimationMontage";
            case ignite::AssetType::Material: return "Material";
            case ignite::AssetType::ScriptableObject: return "ScriptableObject";
            case ignite::AssetType::Audio: return "Audio";
            case ignite::AssetType::Model: return "Model";
            case ignite::AssetType::Font: return "Font";
            case ignite::AssetType::Project: return "Project";
            case ignite::AssetType::TextureCube: return "TextureCube";
            case ignite::AssetType::Scene: return "Scene";
            case ignite::AssetType::InputMapping: return "InputMapping";
            case ignite::AssetType::SkeletalAnimation: return "SkeletalAnimation";
            case ignite::AssetType::SpriteSheet: return "SpriteSheet";
            case ignite::AssetType::Anim2D: return "Anim2D";
			case ignite::AssetType::Mesh: return "Mesh";
			case ignite::AssetType::StaticMesh: return "StaticMesh";
			case ignite::AssetType::SkeletalMesh: return "SkeletalMesh";
            case ignite::AssetType::Skeleton: return "Skeleton";
            case ignite::AssetType::Environment: return "Environment";
            case ignite::AssetType::BlendSpace: return "BlendSpace";
            case ignite::AssetType::LocomotionController: return "LocomotionController";
            case ignite::AssetType::Material2D: return "Material2D";
            case ignite::AssetType::Animation2D: return "Animation2D";
            case ignite::AssetType::AnimatorController: return "AnimatorController";
            case ignite::AssetType::AnimatorController2D: return "AnimatorController2D";
            case ignite::AssetType::Invalid:
            default: return "Invalid";
        }
    }

    static inline AssetType AssetTypeFromString(const std::string &typeStr)
    {
        if (typeStr == "Metadata") return AssetType::Metadata;
        if (typeStr == "Shader") return AssetType::Shader;
        if (typeStr == "ScriptableObject") return AssetType::ScriptableObject;
        if (typeStr == "Widget") return AssetType::Widget;
        if (typeStr == "AnimationMontage") return AssetType::AnimationMontage;
        if (typeStr == "Scene") return AssetType::Scene;
        if (typeStr == "Texture") return AssetType::Texture;
        if (typeStr == "TextureCube") return AssetType::TextureCube;
        if (typeStr == "Audio") return AssetType::Audio;
        if (typeStr == "Project") return AssetType::Project;
        if (typeStr == "Model") return AssetType::Model;
        if (typeStr == "SkeletalAnimation") return AssetType::SkeletalAnimation;
        if (typeStr == "SpriteSheet") return AssetType::SpriteSheet;
        if (typeStr == "Anim2D")  return AssetType::Anim2D;
        if (typeStr == "Mesh")  return AssetType::Mesh;
        if (typeStr == "StaticMesh")  return AssetType::StaticMesh;
        if (typeStr == "SkeletalMesh")  return AssetType::SkeletalMesh;
        if (typeStr == "Skeleton")  return AssetType::Skeleton;
        if (typeStr == "Material")  return AssetType::Material;
        if (typeStr == "InputMapping")  return AssetType::InputMapping;
        if (typeStr == "Environment")  return AssetType::Environment;
        if (typeStr == "BlendSpace")  return AssetType::BlendSpace;
        if (typeStr == "LocomotionController")  return AssetType::LocomotionController;
        if (typeStr == "Font")  return AssetType::Font;
        if (typeStr == "Material2D")  return AssetType::Material2D;
        if (typeStr == "Animation2D")  return AssetType::Animation2D;
        if (typeStr == "AnimatorController")  return AssetType::AnimatorController;
        if (typeStr == "AnimatorController2D")  return AssetType::AnimatorController2D;
        if (typeStr == "ScriptableObject")       return AssetType::ScriptableObject;
        return AssetType::Invalid;
    }

    static inline std::map<std::string, AssetType> s_AssetExtensionMap =
    {
        { ".meta", AssetType::Metadata },
        { ".hlsl", AssetType::Shader },
        { ".mtg", AssetType::AnimationMontage },
        { ".wdgt", AssetType::Widget },
        { ".ixproj", AssetType::Project },
        { ".ixscene", AssetType::Scene },
        { ".ixso", AssetType::ScriptableObject },
        
        // Textures
        { ".jpg", AssetType::Texture },
        { ".png", AssetType::Texture },
        { ".jpeg", AssetType::Texture },
        { ".exr", AssetType::Texture },
        { ".hdr", AssetType::Texture },

        { ".ixsp", AssetType::SpriteSheet },
        
        // Fonts
        { ".otf", AssetType::Font },
        { ".ttf", AssetType::Font },
        
        // audio
        { ".mp3", AssetType::Audio },
        { ".flac", AssetType::Audio },
        { ".wav", AssetType::Audio },

        // Meshes
        { ".fbx", AssetType::Mesh },
        { ".gltf", AssetType::Mesh },
        { ".glb", AssetType::Mesh },
        { ".mesh", AssetType::StaticMesh },
        { ".skmesh", AssetType::SkeletalMesh },

        // Animations
        { ".ixanim", AssetType::SkeletalAnimation},
        { ".ixloco", AssetType::LocomotionController},
        { ".ixskel", AssetType::Skeleton},
        { ".bsp", AssetType::BlendSpace},

        { ".ixmat", AssetType::Material},
        { ".ixenv", AssetType::Environment},

        { ".ixinput", AssetType::InputMapping},

        { ".ixmat2d", AssetType::Material2D},
        { ".anim2d", AssetType::Animation2D},
        { ".ac",   AssetType::AnimatorController},
        { ".ac2d",   AssetType::AnimatorController2D},
    };

    // Binary Extensions
    static inline std::string GetAssetExtensionFromType(const AssetType type)
    {
        switch (type)
        {
        case AssetType::Metadata: return ".meta";
        case AssetType::Shader: return ".hlsl";
        case AssetType::AnimationMontage: return ".mtg";
        case AssetType::Widget: return ".wdgt";
        case AssetType::StaticMesh: return ".mesh";
        case AssetType::SkeletalMesh: return ".skmesh";
        case AssetType::ScriptableObject: return ".ixso";
        case AssetType::Skeleton: return ".ixskel";
        case AssetType::SkeletalAnimation: return ".ixanim";
        case AssetType::BlendSpace: return ".bsp";
        case AssetType::LocomotionController: return ".ixloco";
        case AssetType::InputMapping: return ".ixinput";
        case AssetType::Scene: return ".ixscene";
        case AssetType::Project: return ".ixproj";
        case AssetType::SpriteSheet: return ".ixsp";
        case AssetType::Material: return ".ixmat";
        case AssetType::Material2D: return ".ixmat2d";
        case AssetType::Environment: return ".ixenv";
        case AssetType::Animation2D: return ".anim2d";
        case AssetType::AnimatorController: return ".ac";
        case AssetType::AnimatorController2D: return ".ac2d";
        default: return ".invalid";
        }
    }

    static AssetType GetAssetTypeFromExtension(const std::string &ext)
    {
        if (s_AssetExtensionMap.contains(ext))
        {
            return s_AssetExtensionMap.at(ext);
        }

        return AssetType::Invalid;
    }

	struct AssetResolveKey
	{
		Project *project = nullptr;
		AssetHandle handle = AssetHandle(0);

		bool operator==(const AssetResolveKey &other) const noexcept
		{
			return project == other.project && handle == other.handle;
		}
	};

	struct AssetResolveKeyHash
	{
		size_t operator()(const AssetResolveKey &key) const noexcept
		{
			size_t h = std::hash<const void *>{}(key.project);
			h ^= (std::hash<AssetHandle>{}(key.handle) + 0x9e3779b9 + (h << 6) + (h >> 2));
			return h;
		}
	};

    class IGN_API AssetMetaData
    {
    public:
        AssetMetaData() = default;
        AssetMetaData(const ignite::Path &filepath, const AssetType type)
            : filepath(filepath), type(type)
        {
        }

        ignite::Path filepath;
        AssetType type = AssetType::Invalid;
    };

    class IGN_API Asset : public std::enable_shared_from_this<Asset>
    {
    public:
        using MetaSerializer = std::function<void(Serializer &)>;

        AssetHandle handle = AssetHandle(0);

        virtual ~Asset() = default;

        template<typename T>
        Ref<T> As()
        {
            return std::dynamic_pointer_cast<T>(shared_from_this());
        }

        virtual bool Serialize(const ignite::Path &filepath) { return true; }
        virtual bool SerializeMetaFile(const ignite::Path &filepath, const MetaSerializer &customSerializer = nullptr) const;

        virtual AssetType GetAssetType() { return AssetType::Invalid; }

        void NotifyChange();

        void SetDirtyFlag(bool dirty)  { m_Dirty = dirty; }
        bool IsDirty() const  { return m_Dirty; }

        void SetReadyFlag(bool ready) { m_Ready = ready; }
        bool IsReady() const { return m_Ready; }

    protected:
        bool m_Ready = true;
        bool m_Dirty = false;
    };
}

#endif