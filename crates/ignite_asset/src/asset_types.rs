// Copyright (c) 2026 Evangelion Manuhutu

use std::fmt;
use serde::{Serialize, Deserialize};

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum AssetType {
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
}

impl AssetType {
    pub fn as_str(&self) -> &'static str {
        match self {
            AssetType::Invalid => "Invalid",
            AssetType::Metadata => "Metadata",
            AssetType::Auto => "Auto",
            AssetType::Audio => "Audio",
            AssetType::ScriptableObject => "ScriptableObject",
            AssetType::AudioMixer => "AudioMixer",
            AssetType::SoundCue => "SoundCue",
            AssetType::Model => "Model",
            AssetType::Project => "Project",
            AssetType::Texture => "Texture",
            AssetType::SpriteSheet => "SpriteSheet",
            AssetType::Shader => "Shader",
            AssetType::Material => "Material",
            AssetType::Font => "Font",
            AssetType::TextureCube => "TextureCube",
            AssetType::SkeletalAnimation => "SkeletalAnimation",
            AssetType::AnimationMontage => "AnimationMontage",
            AssetType::BlendSpace => "BlendSpace",
            AssetType::LocomotionController => "LocomotionController",
            AssetType::Environment => "Environment",
            AssetType::Anim2D => "Anim2D",
            AssetType::Skeleton => "Skeleton",
            AssetType::Mesh => "Mesh",
            AssetType::StaticMesh => "StaticMesh",
            AssetType::SkeletalMesh => "SkeletalMesh",
            AssetType::InputMapping => "InputMapping",
            AssetType::Scene => "Scene",
            AssetType::Widget => "Widget",
            AssetType::AnimatorController => "AnimatorController",
            AssetType::Material2D => "Material2D",
            AssetType::Animation2D => "Animation2D",
            AssetType::AnimatorController2D => "AnimatorController2D",
            AssetType::Prefab => "Prefab",
            AssetType::Terrain => "Terrain",
            AssetType::LuaScript => "LuaScript",
        }
    }

    pub fn from_str_name(s: &str) -> AssetType {
        match s {
            "Metadata" => AssetType::Metadata,
            "Auto" => AssetType::Auto,
            "Audio" => AssetType::Audio,
            "ScriptableObject" => AssetType::ScriptableObject,
            "AudioMixer" => AssetType::AudioMixer,
            "SoundCue" => AssetType::SoundCue,
            "Model" => AssetType::Model,
            "Project" => AssetType::Project,
            "Texture" => AssetType::Texture,
            "SpriteSheet" => AssetType::SpriteSheet,
            "Shader" => AssetType::Shader,
            "Material" => AssetType::Material,
            "Font" => AssetType::Font,
            "TextureCube" => AssetType::TextureCube,
            "SkeletalAnimation" => AssetType::SkeletalAnimation,
            "AnimationMontage" => AssetType::AnimationMontage,
            "BlendSpace" => AssetType::BlendSpace,
            "LocomotionController" => AssetType::LocomotionController,
            "Environment" => AssetType::Environment,
            "Anim2D" => AssetType::Anim2D,
            "Skeleton" => AssetType::Skeleton,
            "Mesh" => AssetType::Mesh,
            "StaticMesh" => AssetType::StaticMesh,
            "SkeletalMesh" => AssetType::SkeletalMesh,
            "InputMapping" => AssetType::InputMapping,
            "Scene" => AssetType::Scene,
            "Widget" => AssetType::Widget,
            "AnimatorController" => AssetType::AnimatorController,
            "Material2D" => AssetType::Material2D,
            "Animation2D" => AssetType::Animation2D,
            "AnimatorController2D" => AssetType::AnimatorController2D,
            "Prefab" => AssetType::Prefab,
            "Terrain" => AssetType::Terrain,
            "LuaScript" => AssetType::LuaScript,
            _ => AssetType::Invalid,
        }
    }
}

impl fmt::Display for AssetType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}
