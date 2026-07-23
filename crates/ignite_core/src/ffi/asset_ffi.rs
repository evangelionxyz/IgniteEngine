// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use ignite_asset::{AssetHandle, AssetType};

// Asset
#[unsafe(no_mangle)]
pub extern "C" fn ignite_asset_handle_create(id: u64) -> u64 {
    AssetHandle::from_u64(id).0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_asset_handle_is_valid(handle: u64) -> bool {
    AssetHandle::from_u64(handle).is_valid()
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_asset_type_to_string(asset_type: AssetType) -> *const c_char {
    let s = asset_type.as_str();
    match s {
        "Invalid" => "Invalid\0".as_ptr() as *const c_char,
        "Metadata" => "Metadata\0".as_ptr() as *const c_char,
        "Auto" => "Auto\0".as_ptr() as *const c_char,
        "Audio" => "Audio\0".as_ptr() as *const c_char,
        "ScriptableObject" => "ScriptableObject\0".as_ptr() as *const c_char,
        "AudioMixer" => "AudioMixer\0".as_ptr() as *const c_char,
        "SoundCue" => "SoundCue\0".as_ptr() as *const c_char,
        "Model" => "Model\0".as_ptr() as *const c_char,
        "Project" => "Project\0".as_ptr() as *const c_char,
        "Texture" => "Texture\0".as_ptr() as *const c_char,
        "SpriteSheet" => "SpriteSheet\0".as_ptr() as *const c_char,
        "Shader" => "Shader\0".as_ptr() as *const c_char,
        "Material" => "Material\0".as_ptr() as *const c_char,
        "Font" => "Font\0".as_ptr() as *const c_char,
        "TextureCube" => "TextureCube\0".as_ptr() as *const c_char,
        "SkeletalAnimation" => "SkeletalAnimation\0".as_ptr() as *const c_char,
        "AnimationMontage" => "AnimationMontage\0".as_ptr() as *const c_char,
        "BlendSpace" => "BlendSpace\0".as_ptr() as *const c_char,
        "LocomotionController" => "LocomotionController\0".as_ptr() as *const c_char,
        "Environment" => "Environment\0".as_ptr() as *const c_char,
        "Anim2D" => "Anim2D\0".as_ptr() as *const c_char,
        "Skeleton" => "Skeleton\0".as_ptr() as *const c_char,
        "Mesh" => "Mesh\0".as_ptr() as *const c_char,
        "StaticMesh" => "StaticMesh\0".as_ptr() as *const c_char,
        "SkeletalMesh" => "SkeletalMesh\0".as_ptr() as *const c_char,
        "InputMapping" => "InputMapping\0".as_ptr() as *const c_char,
        "Scene" => "Scene\0".as_ptr() as *const c_char,
        "Widget" => "Widget\0".as_ptr() as *const c_char,
        "AnimatorController" => "AnimatorController\0".as_ptr() as *const c_char,
        "Material2D" => "Material2D\0".as_ptr() as *const c_char,
        "Animation2D" => "Animation2D\0".as_ptr() as *const c_char,
        "AnimatorController2D" => "AnimatorController2D\0".as_ptr() as *const c_char,
        "Prefab" => "Prefab\0".as_ptr() as *const c_char,
        _ => "Unknown\0".as_ptr() as *const c_char,
    }
}
