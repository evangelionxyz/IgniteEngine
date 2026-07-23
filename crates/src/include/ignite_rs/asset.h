// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_ASSET_H
#define IGN_RS_ASSET_H

#include <stdint.h>
#include <stdbool.h>

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

// FFI Exported Functions
uint64_t ignite_asset_handle_create(uint64_t id);
bool ignite_asset_handle_is_valid(uint64_t handle);
const char* ignite_asset_type_to_string(AssetType_RS asset_type);

#ifdef __cplusplus
}
#endif

#endif