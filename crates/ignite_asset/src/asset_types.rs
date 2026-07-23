// Copyright (c) 2026 Evangelion Manuhutu

use std::fmt;

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
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
        }
    }
}

impl fmt::Display for AssetType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}
