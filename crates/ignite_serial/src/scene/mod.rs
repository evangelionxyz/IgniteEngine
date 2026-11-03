// Copyright (c) 2026 Evangelion Manuhutu

pub mod components;
pub use components::*;

use ignite_ecs::EntityId;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct EntitySerial {
    #[serde(rename = "ID")]
    pub id: EntityId,
    #[serde(rename = "Name", default)]
    pub name: String,
    #[serde(rename = "Type", default)]
    pub entity_type: String,
    #[serde(rename = "Parent", default)]
    pub parent: EntityId,

    // Core & Transform
    #[serde(rename = "Transform", skip_serializing_if = "Option::is_none", default)]
    pub transform: Option<TransformComponentSerial>,
    #[serde(rename = "Rendering", skip_serializing_if = "Option::is_none", default)]
    pub rendering: Option<RenderingComponentSerial>,

    // Lighting 3D
    #[serde(rename = "DirectionalLight", skip_serializing_if = "Option::is_none", default)]
    pub directional_light: Option<DirectionalLightComponentSerial>,
    #[serde(rename = "PointLight", skip_serializing_if = "Option::is_none", default)]
    pub point_light: Option<PointLightComponentSerial>,
    #[serde(rename = "SpotLight", skip_serializing_if = "Option::is_none", default)]
    pub spot_light: Option<SpotLightComponentSerial>,

    // Camera
    #[serde(rename = "Camera", skip_serializing_if = "Option::is_none", default)]
    pub camera: Option<CameraComponentSerial>,

    // Meshes
    #[serde(rename = "StaticMesh", skip_serializing_if = "Option::is_none", default)]
    pub static_mesh: Option<StaticMeshComponentSerial>,
    #[serde(rename = "SkeletalMesh", skip_serializing_if = "Option::is_none", default)]
    pub skeletal_mesh: Option<SkeletalMeshComponentSerial>,

    // Physics 3D
    #[serde(rename = "Rigidbody", skip_serializing_if = "Option::is_none", default)]
    pub rigidbody: Option<RigidbodyComponentSerial>,
    #[serde(rename = "BoxCollider", skip_serializing_if = "Option::is_none", default)]
    pub box_collider: Option<BoxColliderComponentSerial>,
    #[serde(rename = "SphereCollider", skip_serializing_if = "Option::is_none", default)]
    pub sphere_collider: Option<SphereColliderComponentSerial>,
    #[serde(rename = "CapsuleCollider", skip_serializing_if = "Option::is_none", default)]
    pub capsule_collider: Option<CapsuleColliderComponentSerial>,
    #[serde(rename = "PlaneCollider", skip_serializing_if = "Option::is_none", default)]
    pub plane_collider: Option<PlaneColliderComponentSerial>,
    #[serde(rename = "MeshCollider", skip_serializing_if = "Option::is_none", default)]
    pub mesh_collider: Option<MeshColliderComponentSerial>,
    #[serde(rename = "HeightFieldCollider", skip_serializing_if = "Option::is_none", default)]
    pub height_field_collider: Option<HeightFieldColliderComponentSerial>,
    #[serde(rename = "CharacterController", skip_serializing_if = "Option::is_none", default)]
    pub character_controller: Option<CharacterControllerComponentSerial>,
    #[serde(rename = "PhysicsJoint", skip_serializing_if = "Option::is_none", default)]
    pub physics_joint: Option<PhysicsJointComponentSerial>,

    // 2D Components
    #[serde(rename = "Sprite2D", skip_serializing_if = "Option::is_none", default)]
    pub sprite_2d: Option<Sprite2DComponentSerial>,
    #[serde(rename = "Animator2D", skip_serializing_if = "Option::is_none", default)]
    pub animator_2d: Option<Animator2DComponentSerial>,
    #[serde(rename = "Circle2D", skip_serializing_if = "Option::is_none", default)]
    pub circle_2d: Option<Circle2DComponentSerial>,
    #[serde(rename = "PointLight2D", skip_serializing_if = "Option::is_none", default)]
    pub point_light_2d: Option<PointLight2DComponentSerial>,
    #[serde(rename = "Rigidbody2D", skip_serializing_if = "Option::is_none", default)]
    pub rigidbody_2d: Option<Rigidbody2DComponentSerial>,
    #[serde(rename = "BoxCollider2D", skip_serializing_if = "Option::is_none", default)]
    pub box_collider_2d: Option<BoxCollider2DComponentSerial>,
    #[serde(rename = "CircleCollider2D", skip_serializing_if = "Option::is_none", default)]
    pub circle_collider_2d: Option<CircleCollider2DComponentSerial>,

    // Environment & Terrain
    #[serde(rename = "Terrain", skip_serializing_if = "Option::is_none", default)]
    pub terrain: Option<TerrainComponentSerial>,
    #[serde(rename = "WorldEnvironment", skip_serializing_if = "Option::is_none", default)]
    pub world_environment: Option<WorldEnvironmentComponentSerial>,
    #[serde(rename = "ParticleSystem", skip_serializing_if = "Option::is_none", default)]
    pub particle_system: Option<ParticleSystemComponentSerial>,

    // Audio & UI & Script
    #[serde(rename = "AudioSource", skip_serializing_if = "Option::is_none", default)]
    pub audio_source: Option<AudioSourceComponentSerial>,
    #[serde(rename = "TextComponent", skip_serializing_if = "Option::is_none", default)]
    pub text_component: Option<TextComponentSerial>,
    #[serde(rename = "WidgetComponent", skip_serializing_if = "Option::is_none", default)]
    pub widget_component: Option<WidgetComponentSerial>,
    #[serde(rename = "Script", skip_serializing_if = "Option::is_none", default)]
    pub script: Option<ScriptComponentSerial>,
    #[serde(rename = "LuaScript", skip_serializing_if = "Option::is_none", default)]
    pub lua_script: Option<LuaScriptComponentSerial>,
    #[serde(rename = "PrefabComponent", skip_serializing_if = "Option::is_none", default)]
    pub prefab: Option<PrefabComponentSerial>,

    // Network
    #[serde(rename = "NetworkIdentityComponent", skip_serializing_if = "Option::is_none", default)]
    pub network_identity: Option<NetworkIdentityComponentSerial>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct SceneSerial {
    #[serde(rename = "Version")]
    pub version: u32,
    #[serde(rename = "Entities", default)]
    pub entities: Vec<EntitySerial>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize, Default)]
pub struct SceneFile {
    #[serde(rename = "Scene")]
    pub scene: SceneSerial,
}

impl SceneFile {
    pub fn new(version: u32) -> Self {
        Self {
            scene: SceneSerial {
                version,
                entities: Vec::new(),
            },
        }
    }

    pub fn from_json(json_str: &str) -> Result<Self, serde_json::Error> {
        serde_json::from_str(json_str)
    }

    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(self)
    }

    pub fn to_json_pretty(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(self)
    }

    pub fn from_file<P: AsRef<Path>>(path: P) -> Result<Self, std::io::Error> {
        let content = fs::read_to_string(path)?;
        Self::from_json(&content).map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))
    }

    pub fn to_file<P: AsRef<Path>>(&self, path: P) -> Result<(), std::io::Error> {
        let content = self.to_json_pretty().map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
        fs::write(path, content)
    }

    /// Converts this SceneFile into a runtime ignite_ecs::Scene instance.
    pub fn to_ecs_scene(&self) -> ignite_ecs::Scene {
        let mut ecs = ignite_ecs::Scene::new("Scene");
        for e in &self.scene.entities {
            ecs.create_entity_with_id(e.id, &e.name);
            if let Some(ref t) = e.transform {
                ecs.set_transform(
                    e.id,
                    ignite_ecs::TransformComponent::new(
                        t.local_translation,
                        [0.0, 0.0, 0.0],
                        t.local_scale,
                    ),
                );
            }
            if let Some(ref s) = e.script {
                ecs.add_script(e.id, ignite_ecs::ScriptComponent::new(&s.class_name));
            }
        }
        ecs
    }

    /// Converts the scene entity hierarchy into the JSON structure expected by UI/Editor
    /// identical to C++ `SceneSerializer::DeserializeHierarchyJson`.
    pub fn to_hierarchy_json(&self) -> Result<String, serde_json::Error> {
        #[derive(Serialize)]
        struct HierarchyNode {
            id: String,
            #[serde(rename = "rawId")]
            raw_id: u64,
            name: String,
            #[serde(rename = "parentId")]
            parent_id: Option<String>,
            #[serde(rename = "isActive")]
            is_active: bool,
            components: Vec<serde_json::Value>,
            children: Vec<HierarchyNode>,
        }

        struct IntermediateEntry<'a> {
            entity: &'a EntitySerial,
            components: Vec<serde_json::Value>,
            child_ids: Vec<u64>,
        }

        let mut entries: HashMap<u64, IntermediateEntry> = HashMap::new();
        let mut root_ids: Vec<u64> = Vec::new();
        let mut all_ids: Vec<u64> = Vec::new();

        for entity in &self.scene.entities {
            let mut comps: Vec<serde_json::Value> = Vec::new();

            if let Some(ref t) = entity.transform {
                comps.push(serde_json::json!({
                    "type": "Transform",
                    "displayName": "Transform",
                    "position": t.local_translation,
                    "rotation": t.local_rotation,
                    "scale": t.local_scale
                }));
            }
            if let Some(ref c) = entity.camera {
                comps.push(serde_json::json!({
                    "type": "Camera",
                    "displayName": "Camera",
                    "isPerspective": c.projection_type == 1,
                    "fov": c.fov,
                    "near": c.near_clip,
                    "far": c.far_clip,
                    "orthoSize": c.ortho_size
                }));
            }
            if let Some(ref l) = entity.directional_light {
                comps.push(serde_json::json!({
                    "type": "DirectionalLight",
                    "displayName": "Directional Light",
                    "color": l.color,
                    "intensity": l.intensity,
                    "shadowDistance": l.shadow_distance,
                    "castShadows": l.cascade_shadow
                }));
            }
            if let Some(ref sm) = entity.static_mesh {
                comps.push(serde_json::json!({
                    "type": "StaticMesh",
                    "displayName": "Static Mesh",
                    "handle": sm.handle
                }));
            }
            if let Some(ref rb) = entity.rigidbody {
                comps.push(serde_json::json!({
                    "type": "Rigidbody",
                    "displayName": "Rigid Body",
                    "bodyType": rb.body_type,
                    "mass": rb.mass,
                    "useGravity": rb.use_gravity
                }));
            }
            if let Some(ref bc) = entity.box_collider {
                comps.push(serde_json::json!({
                    "type": "BoxCollider",
                    "displayName": "Box Collider",
                    "size": bc.scale,
                    "center": bc.center
                }));
            }
            if let Some(ref sc) = entity.sphere_collider {
                comps.push(serde_json::json!({
                    "type": "SphereCollider",
                    "displayName": "Sphere Collider",
                    "radius": sc.radius,
                    "center": sc.center
                }));
            }
            if let Some(ref mc) = entity.mesh_collider {
                comps.push(serde_json::json!({
                    "type": "MeshCollider",
                    "displayName": "Mesh Collider",
                    "center": mc.center,
                    "convex": mc.convex
                }));
            }
            if let Some(ref pj) = entity.physics_joint {
                comps.push(serde_json::json!({
                    "type": "PhysicsJoint",
                    "displayName": "Physics Joint",
                    "jointType": pj.joint_type,
                    "connectedEntity": pj.connected_entity
                }));
            }
            if let Some(ref ps) = entity.particle_system {
                comps.push(serde_json::json!({
                    "type": "ParticleSystem",
                    "displayName": "Particle System",
                    "maxParticles": ps.max_particles,
                    "emissionRate": ps.emission_rate
                }));
            }
            if let Some(ref we) = entity.world_environment {
                comps.push(serde_json::json!({
                    "type": "WorldEnvironment",
                    "displayName": "World Environment",
                    "exposure": we.exposure,
                    "gamma": we.gamma
                }));
            }

            all_ids.push(entity.id.0);
            entries.insert(
                entity.id.0,
                IntermediateEntry {
                    entity,
                    components: comps,
                    child_ids: Vec::new(),
                },
            );
        }

        // Parent-child linkage
        for &id in &all_ids {
            let parent_id = entries.get(&id).map(|e| e.entity.parent.0).unwrap_or(0);
            if parent_id != 0 && entries.contains_key(&parent_id) {
                if let Some(parent_entry) = entries.get_mut(&parent_id) {
                    parent_entry.child_ids.push(id);
                }
            } else {
                root_ids.push(id);
            }
        }

        fn build_hierarchy(id: u64, entries: &HashMap<u64, IntermediateEntry>) -> HierarchyNode {
            let entry = &entries[&id];
            let parent_id_str = if entry.entity.parent.0 != 0 {
                Some(entry.entity.parent.0.to_string())
            } else {
                None
            };

            let children = entry
                .child_ids
                .iter()
                .map(|&child_id| build_hierarchy(child_id, entries))
                .collect();

            HierarchyNode {
                id: entry.entity.id.0.to_string(),
                raw_id: entry.entity.id.0,
                name: entry.entity.name.clone(),
                parent_id: parent_id_str,
                is_active: true,
                components: entry.components.clone(),
                children,
            }
        }

        let roots: Vec<HierarchyNode> = root_ids
            .into_iter()
            .map(|id| build_hierarchy(id, &entries))
            .collect();

        serde_json::to_string(&roots)
    }
}

// -------------------------------------------------
// Tests
// -------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_empty_scene_serialization() {
        let scene = SceneFile::new(4096);
        let json_str = scene.to_json_pretty().unwrap();
        assert!(json_str.contains("\"Version\": 4096"));
        assert!(json_str.contains("\"Entities\": []"));

        let loaded = SceneFile::from_json(&json_str).unwrap();
        assert_eq!(loaded.scene.version, 4096);
        assert_eq!(loaded.scene.entities.len(), 0);
    }

    #[test]
    fn test_deserialize_test_scene_file() {
        // Find Test_Scene.ixscene path relative to crate or repo
        let candidates = [
            "../../ignite/test/test-resources/Test_Scene.ixscene",
            "ignite/test/test-resources/Test_Scene.ixscene",
            "test-resources/Test_Scene.ixscene",
        ];

        let path = candidates
            .iter()
            .find(|p| Path::new(p).exists())
            .expect("Test_Scene.ixscene file should exist in the repository");

        let scene_file = SceneFile::from_file(path).expect("Failed to parse Test_Scene.ixscene");

        assert_eq!(scene_file.scene.version, 4096);
        assert_eq!(scene_file.scene.entities.len(), 20);

        // Verify Box A
        let box_a = scene_file
            .scene
            .entities
            .iter()
            .find(|e| e.name == "Box A")
            .expect("Box A entity should exist");
        assert_eq!(box_a.id.0, 16854569830308731307);
        assert!(box_a.transform.is_some());
        assert!(box_a.rendering.is_some());
        assert!(box_a.static_mesh.is_some());
        assert!(box_a.rigidbody.is_some());
        assert!(box_a.box_collider.is_some());
        assert!(box_a.physics_joint.is_some());

        let t = box_a.transform.as_ref().unwrap();
        assert_eq!(t.world_translation[0], 8.499999);
        assert_eq!(t.world_scale, [1.0, 1.0, 1.0]);

        // Verify Duck with MeshCollider
        let duck_mesh = scene_file
            .scene
            .entities
            .iter()
            .find(|e| e.name == "Duck" && e.mesh_collider.is_some())
            .expect("Duck with MeshCollider should exist");
        let mc = duck_mesh.mesh_collider.as_ref().unwrap();
        assert!(!mc.vertices.is_empty(), "MeshCollider vertices should not be empty");
        assert!(!mc.indices.is_empty(), "MeshCollider indices should not be empty");

        // Verify Main Camera
        let camera_entity = scene_file
            .scene
            .entities
            .iter()
            .find(|e| e.name == "Main Camera")
            .expect("Main Camera entity should exist");
        assert!(camera_entity.camera.is_some());
        let cam = camera_entity.camera.as_ref().unwrap();
        assert_eq!(cam.fov, 45.0);
        assert!(cam.primary);
        assert!((cam.lens.focal_length - 50.3).abs() < 0.1);
        assert_eq!(cam.post_processing.bloom_iterations, 6);

        // Verify World Environment
        let world_env = scene_file
            .scene
            .entities
            .iter()
            .find(|e| e.name == "World Environment")
            .expect("World Environment should exist");
        assert!(world_env.world_environment.is_some());
        let we = world_env.world_environment.as_ref().unwrap();
        assert_eq!(we.fog_start, 28.7);
        assert_eq!(we.fog_end, 100.0);
        assert_eq!(we.planet_radius, 6300.0);

        // Verify Fire Particle
        let fire_particle = scene_file
            .scene
            .entities
            .iter()
            .find(|e| e.name == "Fire Particle")
            .expect("Fire Particle should exist");
        assert!(fire_particle.particle_system.is_some());
        let ps = fire_particle.particle_system.as_ref().unwrap();
        assert_eq!(ps.max_particles, 5000);
        assert_eq!(ps.emission_rate, 120.0);

        // Roundtrip serialization
        let serialized_json = scene_file.to_json().expect("Failed to serialize SceneFile");
        let roundtrip_file = SceneFile::from_json(&serialized_json).expect("Failed to deserialize roundtrip");
        assert_eq!(roundtrip_file.scene.version, scene_file.scene.version);
        assert_eq!(roundtrip_file.scene.entities.len(), scene_file.scene.entities.len());

        // Hierarchy generation
        let hierarchy_json = scene_file.to_hierarchy_json().expect("Failed to generate hierarchy");
        assert!(hierarchy_json.starts_with('['));
        assert!(hierarchy_json.contains("\"Box A\""));
        assert!(hierarchy_json.contains("\"Main Camera\""));
    }
}
