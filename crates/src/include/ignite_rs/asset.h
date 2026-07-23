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

// Handle Primitives
uint64_t ignite_asset_handle_create(uint64_t id);
bool ignite_asset_handle_is_valid(uint64_t handle);
const char* ignite_asset_type_to_string(AssetType_RS asset_type);

// Metadata & Asset Registry FFI
IgniteResult ignite_rs_asset_assign_metadata(uint64_t handle, const char* path, AssetType_RS asset_type);
IgniteResult ignite_rs_asset_get_metadata(uint64_t handle, char* out_path_buf, size_t max_len, AssetType_RS* out_type);
IgniteResult ignite_rs_asset_remove_metadata(uint64_t handle);

// Asset Pinning & Lifetime Tracking FFI (Rule 13 & 14)
IgniteResult ignite_rs_asset_pin(uint64_t handle);
IgniteResult ignite_rs_asset_unpin(uint64_t handle);
bool ignite_rs_asset_is_pinned(uint64_t handle);
uint32_t ignite_rs_asset_get_pin_count(uint64_t handle);

#ifdef __cplusplus
}
#endif

#endif