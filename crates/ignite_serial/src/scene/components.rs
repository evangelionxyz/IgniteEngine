// Copyright (c) 2026 Evangelion Manuhutu

use serde::{Deserialize, Serialize};

// -------------------------------------------------
// Helpers for default values
// -------------------------------------------------

fn default_vec3_one() -> [f32; 3] {
    [1.0, 1.0, 1.0]
}

fn default_quat_identity() -> [f32; 4] {
    [0.0, 0.0, 0.0, 1.0]
}

fn default_color_white() -> [f32; 4] {
    [1.0, 1.0, 1.0, 1.0]
}

fn default_true() -> bool {
    true
}

// -------------------------------------------------
// Transform Component
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TransformComponentSerial {
    #[serde(rename = "WorldTranslation", default)]
    pub world_translation: [f32; 3],
    #[serde(rename = "WorldRotation", default = "default_quat_identity")]
    pub world_rotation: [f32; 4],
    #[serde(rename = "WorldScale", default = "default_vec3_one")]
    pub world_scale: [f32; 3],

    #[serde(rename = "LocalTranslation", default)]
    pub local_translation: [f32; 3],
    #[serde(rename = "LocalRotation", default = "default_quat_identity")]
    pub local_rotation: [f32; 4],
    #[serde(rename = "LocalScale", default = "default_vec3_one")]
    pub local_scale: [f32; 3],
}

impl Default for TransformComponentSerial {
    fn default() -> Self {
        Self {
            world_translation: [0.0, 0.0, 0.0],
            world_rotation: default_quat_identity(),
            world_scale: default_vec3_one(),
            local_translation: [0.0, 0.0, 0.0],
            local_rotation: default_quat_identity(),
            local_scale: default_vec3_one(),
        }
    }
}

// -------------------------------------------------
// Rendering Component
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct RenderingComponentSerial {
    #[serde(rename = "Visible", default = "default_true")]
    pub visible: bool,
}

impl Default for RenderingComponentSerial {
    fn default() -> Self {
        Self { visible: true }
    }
}

// -------------------------------------------------
// Lighting Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct DirectionalLightComponentSerial {
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "Intensity", default)]
    pub intensity: f32,
    #[serde(rename = "AngularRadius", default)]
    pub angular_radius: f32,
    #[serde(rename = "ShadowStrength", default)]
    pub shadow_strength: f32,
    #[serde(rename = "ShadowMinBias", default)]
    pub shadow_min_bias: f32,
    #[serde(rename = "ShadowMaxBias", default)]
    pub shadow_max_bias: f32,
    #[serde(rename = "PCFRadius", default)]
    pub pcf_radius: f32,
    #[serde(rename = "ShadowDistance", default)]
    pub shadow_distance: f32,
    #[serde(rename = "ShadowResolution", default)]
    pub shadow_resolution: u32,
    #[serde(rename = "CascadeShadow", default)]
    pub cascade_shadow: bool,
}

impl Default for DirectionalLightComponentSerial {
    fn default() -> Self {
        Self {
            color: default_color_white(),
            intensity: 1.0,
            angular_radius: 0.5,
            shadow_strength: 1.0,
            shadow_min_bias: 0.001,
            shadow_max_bias: 0.005,
            pcf_radius: 1.0,
            shadow_distance: 100.0,
            shadow_resolution: 2048,
            cascade_shadow: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct PointLightComponentSerial {
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "Intensity", default)]
    pub intensity: f32,
    #[serde(rename = "Range", default)]
    pub range: f32,
    #[serde(rename = "Enabled", default = "default_true")]
    pub enabled: bool,
    #[serde(rename = "ConstantAttenuation", default)]
    pub constant_attenuation: f32,
    #[serde(rename = "LinearAttenuation", default)]
    pub linear_attenuation: f32,
    #[serde(rename = "QuadraticAttenuation", default)]
    pub quadratic_attenuation: f32,
}

impl Default for PointLightComponentSerial {
    fn default() -> Self {
        Self {
            color: default_color_white(),
            intensity: 1.0,
            range: 10.0,
            enabled: true,
            constant_attenuation: 1.0,
            linear_attenuation: 0.09,
            quadratic_attenuation: 0.032,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SpotLightComponentSerial {
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "Intensity", default)]
    pub intensity: f32,
    #[serde(rename = "Range", default)]
    pub range: f32,
    #[serde(rename = "Enabled", default = "default_true")]
    pub enabled: bool,
    #[serde(rename = "ConstantAttenuation", default)]
    pub constant_attenuation: f32,
    #[serde(rename = "LinearAttenuation", default)]
    pub linear_attenuation: f32,
    #[serde(rename = "QuadraticAttenuation", default)]
    pub quadratic_attenuation: f32,
    #[serde(rename = "InnerConeAngle", default)]
    pub inner_cone_angle: f32,
    #[serde(rename = "OuterConeAngle", default)]
    pub outer_cone_angle: f32,
}

impl Default for SpotLightComponentSerial {
    fn default() -> Self {
        Self {
            color: default_color_white(),
            intensity: 1.0,
            range: 10.0,
            enabled: true,
            constant_attenuation: 1.0,
            linear_attenuation: 0.09,
            quadratic_attenuation: 0.032,
            inner_cone_angle: 0.5,
            outer_cone_angle: 0.7,
        }
    }
}

// -------------------------------------------------
// Camera Component & Sub-objects
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct LensSerial {
    #[serde(rename = "EnabledDOF", default)]
    pub enabled_dof: bool,
    #[serde(rename = "FocalLength", default)]
    pub focal_length: f32,
    #[serde(rename = "FocalDistance", default)]
    pub focal_distance: f32,
    #[serde(rename = "FStop", default)]
    pub f_stop: f32,
    #[serde(rename = "FocusRange", default)]
    pub focus_range: f32,
    #[serde(rename = "BlurAmount", default)]
    pub blur_amount: f32,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct PostProcessingSerial {
    #[serde(rename = "EnableVignette", default)]
    pub enable_vignette: bool,
    #[serde(rename = "EnableChromAb", default)]
    pub enable_chrom_ab: bool,
    #[serde(rename = "EnableBloom", default)]
    pub enable_bloom: bool,
    #[serde(rename = "EnableSSAO", default)]
    pub enable_ssao: bool,
    #[serde(rename = "DebugSSAO", default)]
    pub debug_ssao: bool,

    #[serde(rename = "BloomIntensity", default)]
    pub bloom_intensity: f32,
    #[serde(rename = "BloomThreshold", default)]
    pub bloom_threshold: f32,
    #[serde(rename = "BloomKnee", default)]
    pub bloom_knee: f32,
    #[serde(rename = "BloomRadius", default)]
    pub bloom_radius: f32,
    #[serde(rename = "BloomIterations", default)]
    pub bloom_iterations: u32,

    #[serde(rename = "VignetteRadius", default)]
    pub vignette_radius: f32,
    #[serde(rename = "VignetteSoftness", default)]
    pub vignette_softness: f32,
    #[serde(rename = "VignetteIntensity", default)]
    pub vignette_intensity: f32,
    #[serde(rename = "VignetteColor", default)]
    pub vignette_color: [f32; 3],

    #[serde(rename = "ChromAbAmount", default)]
    pub chrom_ab_amount: f32,
    #[serde(rename = "ChromAbRadial", default)]
    pub chrom_ab_radial: f32,

    #[serde(rename = "AoRadius", default)]
    pub ao_radius: f32,
    #[serde(rename = "AoBias", default)]
    pub ao_bias: f32,
    #[serde(rename = "AoIntensity", default)]
    pub ao_intensity: f32,
    #[serde(rename = "AoPower", default)]
    pub ao_power: f32,

    #[serde(rename = "TAA", default)]
    pub taa: bool,
    #[serde(rename = "TAABlendFactor", default)]
    pub taa_blend_factor: f32,
    #[serde(rename = "MSAA", default)]
    pub msaa: bool,
    #[serde(rename = "MSAASampleCount", default)]
    pub msaa_sample_count: u32,
    #[serde(rename = "TonemapMode", default)]
    pub tonemap_mode: i32,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct CameraComponentSerial {
    #[serde(rename = "ProjectionType", default)]
    pub projection_type: i32,
    #[serde(rename = "AspectRatioPreset", default)]
    pub aspect_ratio_preset: Option<i32>,
    #[serde(rename = "OrthoSize", default)]
    pub ortho_size: f32,
    #[serde(rename = "NearClip", default)]
    pub near_clip: f32,
    #[serde(rename = "FarClip", default)]
    pub far_clip: f32,
    #[serde(rename = "Fov", default)]
    pub fov: f32,
    #[serde(rename = "Primary", default)]
    pub primary: bool,
    #[serde(rename = "Lens", default)]
    pub lens: LensSerial,
    #[serde(rename = "PostProcessing", default)]
    pub post_processing: PostProcessingSerial,
}

impl Default for CameraComponentSerial {
    fn default() -> Self {
        Self {
            projection_type: 1,
            aspect_ratio_preset: Some(0),
            ortho_size: 10.0,
            near_clip: 0.1,
            far_clip: 1000.0,
            fov: 60.0,
            primary: true,
            lens: LensSerial::default(),
            post_processing: PostProcessingSerial::default(),
        }
    }
}

// -------------------------------------------------
// Mesh Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct OverrideMaterialSerial {
    #[serde(rename = "Mesh")]
    pub mesh: u32,
    #[serde(rename = "MaterialHandle")]
    pub material_handle: u64,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct SocketAttachmentSerial {
    #[serde(rename = "SocketName")]
    pub socket_name: String,
    #[serde(rename = "MeshHandle")]
    pub mesh_handle: u64,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct StaticMeshComponentSerial {
    #[serde(rename = "Handle", default)]
    pub handle: u64,
    #[serde(rename = "OverrideMaterials", default)]
    pub override_materials: Vec<OverrideMaterialSerial>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct SkeletalMeshComponentSerial {
    #[serde(rename = "Handle", default)]
    pub handle: u64,
    #[serde(rename = "AnimatorHandle", default)]
    pub animator_handle: u64,
    #[serde(rename = "UniqueAnimator", default)]
    pub unique_animator: bool,
    #[serde(rename = "OverrideMaterials", default)]
    pub override_materials: Vec<OverrideMaterialSerial>,
    #[serde(rename = "SocketAttachments", default)]
    pub socket_attachments: Vec<SocketAttachmentSerial>,
}

// -------------------------------------------------
// Physics 3D Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct RigidbodyComponentSerial {
    #[serde(rename = "MotionQuality", default)]
    pub motion_quality: i32,
    #[serde(rename = "BodyType", default)]
    pub body_type: i32,
    #[serde(rename = "Layer", default)]
    pub layer: u32,
    #[serde(rename = "UseGravity", default = "default_true")]
    pub use_gravity: bool,
    #[serde(rename = "RotateX", default = "default_true")]
    pub rotate_x: bool,
    #[serde(rename = "RotateY", default = "default_true")]
    pub rotate_y: bool,
    #[serde(rename = "RotateZ", default = "default_true")]
    pub rotate_z: bool,
    #[serde(rename = "MoveX", default = "default_true")]
    pub move_x: bool,
    #[serde(rename = "MoveY", default = "default_true")]
    pub move_y: bool,
    #[serde(rename = "MoveZ", default = "default_true")]
    pub move_z: bool,
    #[serde(rename = "Mass", default)]
    pub mass: f32,
    #[serde(rename = "LinearVelocity", default)]
    pub linear_velocity: [f32; 3],
    #[serde(rename = "LinearDamping", default)]
    pub linear_damping: f32,
    #[serde(rename = "MaxLinearVelocity", default)]
    pub max_linear_velocity: f32,
    #[serde(rename = "AngularVelocity", default)]
    pub angular_velocity: [f32; 3],
    #[serde(rename = "AngularDamping", default)]
    pub angular_damping: f32,
    #[serde(rename = "MaxAngularVelocity", default)]
    pub max_angular_velocity: f32,
    #[serde(rename = "Restitution", default)]
    pub restitution: f32,
    #[serde(rename = "Friction", default)]
    pub friction: f32,
    #[serde(rename = "ApplyGyroscopicForce", default)]
    pub apply_gyroscopic_force: bool,
    #[serde(rename = "AllowSleeping", default = "default_true")]
    pub allow_sleeping: bool,
    #[serde(rename = "IsSensor", default)]
    pub is_sensor: bool,
    #[serde(rename = "RetainAcceleration", default)]
    pub retain_acceleration: bool,
    #[serde(rename = "GravityFactor", default)]
    pub gravity_factor: f32,
    #[serde(rename = "CenterMass", default)]
    pub center_mass: [f32; 3],
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct BoxColliderComponentSerial {
    #[serde(rename = "Scale", default = "default_vec3_one")]
    pub scale: [f32; 3],
    #[serde(rename = "Center", default)]
    pub center: [f32; 3],
}

impl Default for BoxColliderComponentSerial {
    fn default() -> Self {
        Self {
            scale: default_vec3_one(),
            center: [0.0, 0.0, 0.0],
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct SphereColliderComponentSerial {
    #[serde(rename = "Radius", default)]
    pub radius: f32,
    #[serde(rename = "Center", default)]
    pub center: [f32; 3],
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct CapsuleColliderComponentSerial {
    #[serde(rename = "Radius", default)]
    pub radius: f32,
    #[serde(rename = "Center", default)]
    pub center: [f32; 3],
    #[serde(rename = "Height", default)]
    pub height: f32,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct PlaneColliderComponentSerial {
    #[serde(rename = "Scale", default = "default_vec3_one")]
    pub scale: [f32; 3],
}

impl Default for PlaneColliderComponentSerial {
    fn default() -> Self {
        Self {
            scale: default_vec3_one(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct MeshColliderComponentSerial {
    #[serde(rename = "Center", default)]
    pub center: [f32; 3],
    #[serde(rename = "Convex", default)]
    pub convex: bool,
    #[serde(rename = "Vertices", default)]
    pub vertices: Vec<[f32; 3]>,
    #[serde(rename = "Indices", default)]
    pub indices: Vec<u32>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct HeightFieldColliderComponentSerial {
    #[serde(rename = "Center", default)]
    pub center: [f32; 3],
    #[serde(rename = "Scale", default)]
    pub scale: [f32; 3],
    #[serde(rename = "SampleCount", default)]
    pub sample_count: u32,
    #[serde(rename = "Heights", default)]
    pub heights: Vec<f32>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct CharacterControllerComponentSerial {
    #[serde(rename = "Radius", default)]
    pub radius: f32,
    #[serde(rename = "Height", default)]
    pub height: f32,
    #[serde(rename = "MaxStepHeight", default)]
    pub max_step_height: f32,
    #[serde(rename = "MaxSlopeAngle", default)]
    pub max_slope_angle: f32,
    #[serde(rename = "Mass", default)]
    pub mass: f32,
    #[serde(rename = "Friction", default)]
    pub friction: f32,
    #[serde(rename = "GravityFactor", default)]
    pub gravity_factor: f32,
    #[serde(rename = "Center", default)]
    pub center: [f32; 3],
    #[serde(rename = "Up", default)]
    pub up: [f32; 3],
    #[serde(rename = "LinearVelocity", default)]
    pub linear_velocity: [f32; 3],
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct PhysicsJointComponentSerial {
    #[serde(rename = "ConnectedEntity", default)]
    pub connected_entity: u64,
    #[serde(rename = "Type", default)]
    pub joint_type: String,
    #[serde(rename = "Space", default)]
    pub space: String,
    #[serde(rename = "Enabled", default = "default_true")]
    pub enabled: bool,
    #[serde(rename = "AutoConfigureConnected", default)]
    pub auto_configure_connected: bool,
    #[serde(rename = "LocalAnchorA", default)]
    pub local_anchor_a: [f32; 3],
    #[serde(rename = "LocalAxisA", default)]
    pub local_axis_a: [f32; 3],
    #[serde(rename = "LocalNormalA", default)]
    pub local_normal_a: [f32; 3],
    #[serde(rename = "LocalAnchorB", default)]
    pub local_anchor_b: [f32; 3],
    #[serde(rename = "LocalAxisB", default)]
    pub local_axis_b: [f32; 3],
    #[serde(rename = "LocalNormalB", default)]
    pub local_normal_b: [f32; 3],
    #[serde(rename = "BreakForce", default)]
    pub break_force: f32,
    #[serde(rename = "BreakTorque", default)]
    pub break_torque: f32,
    #[serde(rename = "MinDistance", default)]
    pub min_distance: f32,
    #[serde(rename = "MaxDistance", default)]
    pub max_distance: f32,
    #[serde(rename = "EnableLimits", default)]
    pub enable_limits: bool,
    #[serde(rename = "LimitsMin", default)]
    pub limits_min: f32,
    #[serde(rename = "LimitsMax", default)]
    pub limits_max: f32,
    #[serde(rename = "EnableSpring", default)]
    pub enable_spring: bool,
    #[serde(rename = "SpringFrequency", default)]
    pub spring_frequency: f32,
    #[serde(rename = "SpringDamping", default)]
    pub spring_damping: f32,
    #[serde(rename = "MaxFriction", default)]
    pub max_friction: f32,
    #[serde(rename = "MotorState", default)]
    pub motor_state: String,
    #[serde(rename = "MotorTargetVelocity", default)]
    pub motor_target_velocity: f32,
    #[serde(rename = "MotorTargetPosition", default)]
    pub motor_target_position: f32,
    #[serde(rename = "MaxMotorForce", default)]
    pub max_motor_force: f32,
    #[serde(rename = "MotorFrequency", default)]
    pub motor_frequency: f32,
    #[serde(rename = "MotorDamping", default)]
    pub motor_damping: f32,
    #[serde(rename = "HalfConeAngle", default)]
    pub half_cone_angle: f32,
}

// -------------------------------------------------
// 2D Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Sprite2DComponentSerial {
    #[serde(rename = "MaterialHandle", default)]
    pub material_handle: u64,
    #[serde(rename = "Handle", default)]
    pub handle: u64,
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "TilingFactor", default)]
    pub tiling_factor: [f32; 2],
    #[serde(rename = "UV0", default)]
    pub uv0: [f32; 2],
    #[serde(rename = "UV1", default)]
    pub uv1: [f32; 2],
    #[serde(rename = "FlipX", default)]
    pub flip_x: bool,
    #[serde(rename = "FlipY", default)]
    pub flip_y: bool,
}

impl Default for Sprite2DComponentSerial {
    fn default() -> Self {
        Self {
            material_handle: 0,
            handle: 0,
            color: default_color_white(),
            tiling_factor: [1.0, 1.0],
            uv0: [0.0, 0.0],
            uv1: [1.0, 1.0],
            flip_x: false,
            flip_y: false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct Animator2DComponentSerial {
    #[serde(rename = "ControllerHandle", default)]
    pub controller_handle: u64,
    #[serde(rename = "CurrentState", default)]
    pub current_state: String,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct Circle2DComponentSerial {
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "Thickness", default)]
    pub thickness: f32,
    #[serde(rename = "Fade", default)]
    pub fade: f32,
}

impl Default for Circle2DComponentSerial {
    fn default() -> Self {
        Self {
            color: default_color_white(),
            thickness: 1.0,
            fade: 0.005,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct PointLight2DComponentSerial {
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "Radius", default)]
    pub radius: f32,
    #[serde(rename = "Intensity", default)]
    pub intensity: f32,
    #[serde(rename = "Enabled", default = "default_true")]
    pub enabled: bool,
}

impl Default for PointLight2DComponentSerial {
    fn default() -> Self {
        Self {
            color: default_color_white(),
            radius: 5.0,
            intensity: 1.0,
            enabled: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct Rigidbody2DComponentSerial {
    #[serde(rename = "BodyType", default)]
    pub body_type: i32,
    #[serde(rename = "LinearVelocity", default)]
    pub linear_velocity: [f32; 2],
    #[serde(rename = "AngularVelocity", default)]
    pub angular_velocity: f32,
    #[serde(rename = "GravityScale", default)]
    pub gravity_scale: f32,
    #[serde(rename = "LinearDamping", default)]
    pub linear_damping: f32,
    #[serde(rename = "AngularDamping", default)]
    pub angular_damping: f32,
    #[serde(rename = "IsAwake", default = "default_true")]
    pub is_awake: bool,
    #[serde(rename = "FixedRotation", default)]
    pub fixed_rotation: bool,
    #[serde(rename = "AllowFastRotation", default)]
    pub allow_fast_rotation: bool,
    #[serde(rename = "IsEnabled", default = "default_true")]
    pub is_enabled: bool,
    #[serde(rename = "Sleeping", default = "default_true")]
    pub sleeping: bool,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct BoxCollider2DComponentSerial {
    #[serde(rename = "Size", default)]
    pub size: [f32; 2],
    #[serde(rename = "Offset", default)]
    pub offset: [f32; 2],
    #[serde(rename = "Restitution", default)]
    pub restitution: f32,
    #[serde(rename = "Friction", default)]
    pub friction: f32,
    #[serde(rename = "Density", default)]
    pub density: f32,
    #[serde(rename = "IsSensor", default)]
    pub is_sensor: bool,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct CircleCollider2DComponentSerial {
    #[serde(rename = "Radius", default)]
    pub radius: f32,
    #[serde(rename = "Center", default)]
    pub center: [f32; 2],
    #[serde(rename = "Restitution", default)]
    pub restitution: f32,
    #[serde(rename = "Friction", default)]
    pub friction: f32,
    #[serde(rename = "Density", default)]
    pub density: f32,
    #[serde(rename = "IsSensor", default)]
    pub is_sensor: bool,
}

// -------------------------------------------------
// Environment & Terrain Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct TerrainComponentSerial {
    #[serde(rename = "Resolution", default)]
    pub resolution: u32,
    #[serde(rename = "WorldSize", default)]
    pub world_size: f32,
    #[serde(rename = "MaxHeight", default)]
    pub max_height: f32,
    #[serde(rename = "NoiseFreq", default)]
    pub noise_freq: f32,
    #[serde(rename = "NoiseSeed", default)]
    pub noise_seed: i32,
    #[serde(rename = "ChunkCount", default)]
    pub chunk_count: u32,
    #[serde(rename = "LodLevels", default)]
    pub lod_levels: u32,
    #[serde(rename = "HeightmapHandle", default)]
    pub heightmap_handle: u64,
    #[serde(rename = "MaterialHandle", default)]
    pub material_handle: u64,
    #[serde(rename = "Heightmap", skip_serializing_if = "Option::is_none", default)]
    pub heightmap: Option<Vec<f32>>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct WorldEnvironmentComponentSerial {
    #[serde(rename = "SkyType", default)]
    pub sky_type: u32,
    #[serde(rename = "HDRHandle", default)]
    pub hdr_handle: u64,
    #[serde(rename = "Exposure", default)]
    pub exposure: f32,
    #[serde(rename = "Gamma", default)]
    pub gamma: f32,
    #[serde(rename = "Ambient", default)]
    pub ambient: f32,
    #[serde(rename = "FogDensity", default)]
    pub fog_density: f32,
    #[serde(rename = "FogColor", default)]
    pub fog_color: [f32; 4],
    #[serde(rename = "FogStart", default)]
    pub fog_start: f32,
    #[serde(rename = "FogEnd", default)]
    pub fog_end: f32,

    #[serde(rename = "RayleighScattering", default)]
    pub rayleigh_scattering: [f32; 3],
    #[serde(rename = "RayleighDensityH", default)]
    pub rayleigh_density_h: f32,
    #[serde(rename = "MieScattering", default)]
    pub mie_scattering: [f32; 3],
    #[serde(rename = "MieDensityH", default)]
    pub mie_density_h: f32,
    #[serde(rename = "MieG", default)]
    pub mie_g: f32,
    #[serde(rename = "OzoneAbsorption", default)]
    pub ozone_absorption: [f32; 3],
    #[serde(rename = "PlanetRadius", default)]
    pub planet_radius: f32,
    #[serde(rename = "AtmosphereRadius", default)]
    pub atmosphere_radius: f32,
    #[serde(rename = "GroundAlbedo", default)]
    pub ground_albedo: [f32; 3],
}

// -------------------------------------------------
// Particle System Component
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct ParticleSystemComponentSerial {
    #[serde(rename = "IsPlaying", default = "default_true")]
    pub is_playing: bool,
    #[serde(rename = "EmitterType", default)]
    pub emitter_type: u32,
    #[serde(rename = "BlendMode", default)]
    pub blend_mode: u32,
    #[serde(rename = "MaxParticles", default)]
    pub max_particles: u32,
    #[serde(rename = "EmissionRate", default)]
    pub emission_rate: f32,
    #[serde(rename = "LifetimeMin", default)]
    pub lifetime_min: f32,
    #[serde(rename = "LifetimeMax", default)]
    pub lifetime_max: f32,
    #[serde(rename = "VelocityMin", default)]
    pub velocity_min: [f32; 3],
    #[serde(rename = "VelocityMax", default)]
    pub velocity_max: [f32; 3],
    #[serde(rename = "Acceleration", default)]
    pub acceleration: [f32; 3],
    #[serde(rename = "Drag", default)]
    pub drag: f32,
    #[serde(rename = "StartColor", default = "default_color_white")]
    pub start_color: [f32; 4],
    #[serde(rename = "EndColor", default)]
    pub end_color: [f32; 4],
    #[serde(rename = "StartSize", default)]
    pub start_size: [f32; 2],
    #[serde(rename = "EndSize", default)]
    pub end_size: [f32; 2],
    #[serde(rename = "RotationSpeedMin", default)]
    pub rotation_speed_min: f32,
    #[serde(rename = "RotationSpeedMax", default)]
    pub rotation_speed_max: f32,
    #[serde(rename = "SpawnRadius", default)]
    pub spawn_radius: f32,
    #[serde(rename = "VelocityAlignment", default)]
    pub velocity_alignment: f32,
    #[serde(rename = "TextureHandle", default)]
    pub texture_handle: u64,
}

// -------------------------------------------------
// Audio Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct DspSerial {
    #[serde(rename = "Type")]
    pub dsp_type: i32,
    #[serde(rename = "Enabled")]
    pub enabled: bool,

    // Reverb
    #[serde(rename = "DecayTime", skip_serializing_if = "Option::is_none")]
    pub decay_time: Option<f32>,
    #[serde(rename = "EarlyDelay", skip_serializing_if = "Option::is_none")]
    pub early_delay: Option<f32>,
    #[serde(rename = "LateDelay", skip_serializing_if = "Option::is_none")]
    pub late_delay: Option<f32>,
    #[serde(rename = "HighFrequencyReference", skip_serializing_if = "Option::is_none")]
    pub high_frequency_reference: Option<f32>,
    #[serde(rename = "Diffusion", skip_serializing_if = "Option::is_none")]
    pub diffusion: Option<f32>,
    #[serde(rename = "Density", skip_serializing_if = "Option::is_none")]
    pub density: Option<f32>,
    #[serde(rename = "LowShelfFrequency", skip_serializing_if = "Option::is_none")]
    pub low_shelf_frequency: Option<f32>,
    #[serde(rename = "HighCut", skip_serializing_if = "Option::is_none")]
    pub high_cut: Option<f32>,
    #[serde(rename = "DryLevel", skip_serializing_if = "Option::is_none")]
    pub dry_level: Option<f32>,
    #[serde(rename = "WetLevel", skip_serializing_if = "Option::is_none")]
    pub wet_level: Option<f32>,

    // Distortion
    #[serde(rename = "Level", skip_serializing_if = "Option::is_none")]
    pub level: Option<f32>,

    // Chorus
    #[serde(rename = "Mix", skip_serializing_if = "Option::is_none")]
    pub mix: Option<f32>,
    #[serde(rename = "Rate", skip_serializing_if = "Option::is_none")]
    pub rate: Option<f32>,
    #[serde(rename = "Depth", skip_serializing_if = "Option::is_none")]
    pub depth: Option<f32>,

    // Compressor
    #[serde(rename = "Threshold", skip_serializing_if = "Option::is_none")]
    pub threshold: Option<f32>,
    #[serde(rename = "Ratio", skip_serializing_if = "Option::is_none")]
    pub ratio: Option<f32>,
    #[serde(rename = "Release", skip_serializing_if = "Option::is_none")]
    pub release: Option<f32>,
    #[serde(rename = "GainMakeup", skip_serializing_if = "Option::is_none")]
    pub gain_makeup: Option<f32>,
    #[serde(rename = "UseSidechain", skip_serializing_if = "Option::is_none")]
    pub use_sidechain: Option<bool>,

    // Delay
    #[serde(rename = "DelayMs", skip_serializing_if = "Option::is_none")]
    pub delay_ms: Option<f32>,
    #[serde(rename = "Feedback", skip_serializing_if = "Option::is_none")]
    pub feedback: Option<f32>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct AudioSourceComponentSerial {
    #[serde(rename = "Handle", default)]
    pub handle: u64,
    #[serde(rename = "Volume", default)]
    pub volume: f32,
    #[serde(rename = "Pitch", default)]
    pub pitch: f32,
    #[serde(rename = "Pan", default)]
    pub pan: f32,
    #[serde(rename = "PlayOnStart", default)]
    pub play_on_start: bool,
    #[serde(rename = "Loop", default)]
    pub loop_audio: bool,
    #[serde(rename = "DSPs", default)]
    pub dsps: Vec<DspSerial>,
}

impl Default for AudioSourceComponentSerial {
    fn default() -> Self {
        Self {
            handle: 0,
            volume: 1.0,
            pitch: 1.0,
            pan: 0.0,
            play_on_start: false,
            loop_audio: false,
            dsps: Vec::new(),
        }
    }
}

// -------------------------------------------------
// UI & Scripting & Prefab Components
// -------------------------------------------------

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TextComponentSerial {
    #[serde(rename = "FontHandle", default)]
    pub font_handle: u64,
    #[serde(rename = "Material2DHandle", default)]
    pub material2d_handle: u64,
    #[serde(rename = "Text", default)]
    pub text: String,
    #[serde(rename = "Color", default = "default_color_white")]
    pub color: [f32; 4],
    #[serde(rename = "Kerning", default)]
    pub kerning: f32,
    #[serde(rename = "LineSpacing", default)]
    pub line_spacing: f32,
    #[serde(rename = "ScreenSpace", default)]
    pub screen_space: bool,
}

impl Default for TextComponentSerial {
    fn default() -> Self {
        Self {
            font_handle: 0,
            material2d_handle: 0,
            text: String::new(),
            color: default_color_white(),
            kerning: 0.0,
            line_spacing: 0.0,
            screen_space: false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct WidgetComponentSerial {
    #[serde(rename = "WidgetHandle", default)]
    pub widget_handle: u64,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct ScriptFieldSerial {
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "Value")]
    pub value: serde_json::Value,
    #[serde(rename = "Type")]
    pub field_type: String,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct ScriptComponentSerial {
    #[serde(rename = "ClassName", default)]
    pub class_name: String,
    #[serde(rename = "Fields", default)]
    pub fields: Vec<ScriptFieldSerial>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct PrefabComponentSerial {
    #[serde(rename = "PrefabHandle", default)]
    pub prefab_handle: u64,
    #[serde(rename = "PrefabEntityUUID", default)]
    pub prefab_entity_uuid: u64,
}
