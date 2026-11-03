// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_ASSET_TYPES_HPP
#define IGN_CORE_ASSET_TYPES_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/result.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/types.hpp"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <string>
#include <map>
#include <filesystem>

#ifdef __cplusplus
namespace ignite
{
    using AssetHandle = UUID;

    enum class AssetType : uint32_t
    {
        Invalid = 0,
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
        Mesh,
        StaticMesh,
        SkeletalMesh,
        InputMapping,
        Scene,
        Widget,
        AnimatorController,
        Material2D,
        Animation2D,
        AnimatorController2D,
        Prefab,
        Terrain,
        LuaScript,
    };
}

typedef ignite::AssetType Ignite_AssetType;
typedef Ignite_AssetType AssetType_RS;

#else

typedef enum Ignite_AssetType
{
    Ignite_AssetType_Invalid = 0,
    Ignite_AssetType_Metadata,
    Ignite_AssetType_Auto,
    Ignite_AssetType_Audio,
    Ignite_AssetType_ScriptableObject,
    Ignite_AssetType_AudioMixer,
    Ignite_AssetType_SoundCue,
    Ignite_AssetType_Model,
    Ignite_AssetType_Project,
    Ignite_AssetType_Texture,
    Ignite_AssetType_SpriteSheet,
    Ignite_AssetType_Shader,
    Ignite_AssetType_Material,
    Ignite_AssetType_Font,
    Ignite_AssetType_TextureCube,
    Ignite_AssetType_SkeletalAnimation,
    Ignite_AssetType_AnimationMontage,
    Ignite_AssetType_BlendSpace,
    Ignite_AssetType_LocomotionController,
    Ignite_AssetType_Environment,
    Ignite_AssetType_Anim2D,
    Ignite_AssetType_Skeleton,
    Ignite_AssetType_Mesh,
    Ignite_AssetType_StaticMesh,
    Ignite_AssetType_SkeletalMesh,
    Ignite_AssetType_InputMapping,
    Ignite_AssetType_Scene,
    Ignite_AssetType_Widget,
    Ignite_AssetType_AnimatorController,
    Ignite_AssetType_Material2D,
    Ignite_AssetType_Animation2D,
    Ignite_AssetType_AnimatorController2D,
    Ignite_AssetType_Prefab,
    Ignite_AssetType_Terrain,
    Ignite_AssetType_LuaScript,
} Ignite_AssetType;

typedef Ignite_AssetType AssetType_RS;

#endif

// Matching Rust AssetState enum values
typedef enum AssetState_RS
{
    AssetState_RS_Unloaded = 0,
    AssetState_RS_Queued = 1,
    AssetState_RS_Loading = 2,
    AssetState_RS_Ready = 3,
    AssetState_RS_Dirty = 4,
    AssetState_RS_Unloading = 5,
} AssetState_RS;

typedef AssetState_RS Ignite_AssetState;

// FFI struct for bulk snapshot copy
typedef struct IgniteAssetRegistryEntryFFI
{
    uint64_t handle;
    Ignite_AssetType asset_type;
    char filepath[260];
} IgniteAssetRegistryEntryFFI;

#ifdef __cplusplus
extern "C" {
#endif

// Handle Primitives
uint64_t ignite_asset_handle_create(uint64_t id);
bool ignite_asset_handle_is_valid(uint64_t handle);
const char *ignite_asset_type_to_string(Ignite_AssetType asset_type);
Ignite_AssetType ignite_asset_type_from_string(const char *type_str);

// Metadata & Asset Registry FFI
IgniteResult ignite_asset_assign_metadata(uint64_t handle, const char *path, Ignite_AssetType asset_type);
IgniteResult ignite_asset_get_metadata(uint64_t handle, char *out_path_buf, size_t max_len, Ignite_AssetType *out_type);
IgniteResult ignite_asset_remove_metadata(uint64_t handle);

// Bulk Registry Snapshot & Lifecycle FFI
uint64_t ignite_asset_get_registry_version(void);
size_t ignite_asset_get_registry_count(void);
size_t ignite_asset_get_registry_snapshot(IgniteAssetRegistryEntryFFI *out_entries, size_t max_count);
IgniteResult ignite_asset_request_import(uint64_t handle);
size_t ignite_asset_poll_import_requests(uint64_t *out_handles, size_t max_count);
IgniteResult ignite_asset_mark_ready(uint64_t handle);
Ignite_AssetState ignite_asset_get_state(uint64_t handle);
IgniteResult ignite_asset_set_state(uint64_t handle, Ignite_AssetState state);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace ignite
{
    class Project;

    static inline std::string AssetTypeToString(AssetType type)
    {
        const char *str = ignite_asset_type_to_string(type);
        return str ? std::string(str) : "Invalid";
    }

    static inline AssetType AssetTypeFromString(const std::string &typeStr)
    {
        return ignite_asset_type_from_string(typeStr.c_str());
    }

    static inline std::map<std::string, AssetType> s_AssetExtensionMap =
    {
        { ".meta", AssetType::Metadata },
        { ".hlsl", AssetType::Shader },
        { ".mtg", AssetType::AnimationMontage },
        { ".wdgt", AssetType::Widget },
        { ".ixproj", AssetType::Project },
        { ".ixscene", AssetType::Scene },
        { ".ixprefab", AssetType::Prefab },
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

        // Audio
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
        { ".ixanim", AssetType::SkeletalAnimation },
        { ".ixloco", AssetType::LocomotionController },
        { ".ixskel", AssetType::Skeleton },
        { ".bsp", AssetType::BlendSpace },

        { ".ixmat", AssetType::Material },
        { ".ixenv", AssetType::Environment },

        { ".ixinput", AssetType::InputMapping },

        { ".ixmat2d", AssetType::Material2D },
        { ".anim2d", AssetType::Animation2D },
        { ".ac", AssetType::AnimatorController },
        { ".ac2d", AssetType::AnimatorController2D },
        { ".ter", AssetType::Terrain },
        { ".lua", AssetType::LuaScript },
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
            case AssetType::Prefab: return ".ixprefab";
            case AssetType::Project: return ".ixproj";
            case AssetType::SpriteSheet: return ".ixsp";
            case AssetType::Material: return ".ixmat";
            case AssetType::Material2D: return ".ixmat2d";
            case AssetType::Environment: return ".ixenv";
            case AssetType::Animation2D: return ".anim2d";
            case AssetType::AnimatorController: return ".ac";
            case AssetType::AnimatorController2D: return ".ac2d";
            case AssetType::Terrain: return ".ter";
            case AssetType::LuaScript: return ".lua";
            default: return ".invalid";
        }
    }

    static inline AssetType GetAssetTypeFromExtension(const std::string &ext)
    {
        if (s_AssetExtensionMap.contains(ext))
            return s_AssetExtensionMap.at(ext);
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

    struct AssetMetaData
    {
        AssetMetaData() = default;
        AssetMetaData(const std::filesystem::path &filepath, const AssetType type)
            : filepath(filepath), type(type)
        {}

        std::filesystem::path filepath;
        AssetType type = AssetType::Invalid;
    };
}
#endif

#endif // IGN_CORE_ASSET_TYPES_HPP
