// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_ASSET_H
#define IGN_RS_ASSET_H

#include "result.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Matching Rust AssetType enum values
typedef enum AssetType_RS
{
    AssetType_RS_Invalid = 0,
    AssetType_RS_Metadata,
    AssetType_RS_Auto,
    AssetType_RS_Audio,
    AssetType_RS_ScriptableObject,
    AssetType_RS_AudioMixer,
    AssetType_RS_SoundCue,
    AssetType_RS_Model,
    AssetType_RS_Project,
    AssetType_RS_Texture,
    AssetType_RS_SpriteSheet,
    AssetType_RS_Shader,
    AssetType_RS_Material,
    AssetType_RS_Font,
    AssetType_RS_TextureCube,
    AssetType_RS_SkeletalAnimation,
    AssetType_RS_AnimationMontage,
    AssetType_RS_BlendSpace,
    AssetType_RS_LocomotionController,
    AssetType_RS_Environment,
    AssetType_RS_Anim2D,
    AssetType_RS_Skeleton,
    AssetType_RS_Mesh,
    AssetType_RS_StaticMesh,
    AssetType_RS_SkeletalMesh,
    AssetType_RS_InputMapping,
    AssetType_RS_Scene,
    AssetType_RS_Widget,
    AssetType_RS_AnimatorController,
    AssetType_RS_Material2D,
    AssetType_RS_Animation2D,
    AssetType_RS_AnimatorController2D,
    AssetType_RS_Prefab,
} AssetType_RS;

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

// FFI struct for bulk snapshot copy
typedef struct IgniteAssetRegistryEntryFFI
{
    uint64_t handle;
    AssetType_RS asset_type;
    char filepath[260];
} IgniteAssetRegistryEntryFFI;

// Handle Primitives
uint64_t ignite_asset_handle_create(uint64_t id);
bool ignite_asset_handle_is_valid(uint64_t handle);
const char* ignite_asset_type_to_string(AssetType_RS asset_type);

// Metadata & Asset Registry FFI
IgniteResult ignite_rs_asset_assign_metadata(uint64_t handle, const char* path, AssetType_RS asset_type);
IgniteResult ignite_rs_asset_get_metadata(uint64_t handle, char* out_path_buf, size_t max_len, AssetType_RS* out_type);
IgniteResult ignite_rs_asset_remove_metadata(uint64_t handle);

// Bulk Registry Snapshot & Lifecycle FFI
uint64_t ignite_rs_asset_get_registry_version(void);
size_t ignite_rs_asset_get_registry_count(void);
size_t ignite_rs_asset_get_registry_snapshot(IgniteAssetRegistryEntryFFI* out_entries, size_t max_count);
IgniteResult ignite_rs_asset_request_import(uint64_t handle);
size_t ignite_rs_asset_poll_import_requests(uint64_t* out_handles, size_t max_count);
IgniteResult ignite_rs_asset_mark_ready(uint64_t handle);
AssetState_RS ignite_rs_asset_get_state(uint64_t handle);
IgniteResult ignite_rs_asset_set_state(uint64_t handle, AssetState_RS state);

#ifdef __cplusplus
}
#endif

#endif