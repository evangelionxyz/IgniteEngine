// Copyright (c) 2026 Evangelion Manuhutu

use std::fs;
use std::path::Path;
use serde::{Serialize, Deserialize};

fn default_version() -> String {
    "0.0.0".to_string()
}

fn default_name() -> String {
    "Untitled".to_string()
}

fn default_asset_path() -> String {
    "Assets".to_string()
}

fn default_asset_reg() -> String {
    "AssetRegistry.ixreg".to_string()
}

fn default_config() -> String {
    "Debug".to_string()
}

fn default_gravity() -> [f32; 3] {
    [0.0, -9.81, 0.0]
}

fn default_layer_count() -> u32 {
    1
}

fn default_port() -> u16 {
    8192
}

fn default_tick_rate() -> u16 {
    60
}

fn default_snapshot_rate() -> u16 {
    20
}

fn default_max_clients() -> u32 {
    32
}

fn default_handshake_timeout() -> f64 {
    5.0
}

fn default_chat_interval() -> f64 {
    0.25
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct PhysicsConfigSerial {
    #[serde(rename = "Gravity", default = "default_gravity")]
    pub gravity: [f32; 3],
    #[serde(rename = "LayerCount", default = "default_layer_count")]
    pub layer_count: u32,
    #[serde(rename = "LayerNames", default)]
    pub layer_names: Vec<String>,
    #[serde(rename = "CollisionMasks", default)]
    pub collision_masks: Vec<u32>,
}

impl Default for PhysicsConfigSerial {
    fn default() -> Self {
        Self {
            gravity: default_gravity(),
            layer_count: default_layer_count(),
            layer_names: vec!["Default".to_string()],
            collision_masks: vec![0xFFFFFFFF],
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ServerConfigSerial {
    #[serde(rename = "Port", default = "default_port")]
    pub port: u16,
    #[serde(rename = "TickRate", default = "default_tick_rate")]
    pub tick_rate: u16,
    #[serde(rename = "SnapshotRate", default = "default_snapshot_rate")]
    pub snapshot_rate: u16,
    #[serde(rename = "MaxClients", default = "default_max_clients")]
    pub max_clients: u32,
    #[serde(rename = "HandshakeTimeout", default = "default_handshake_timeout")]
    pub handshake_timeout: f64,
    #[serde(rename = "ChatMinInterval", default = "default_chat_interval")]
    pub chat_min_interval: f64,
    #[serde(rename = "AutoStart", default)]
    pub auto_start: bool,
}

impl Default for ServerConfigSerial {
    fn default() -> Self {
        Self {
            port: default_port(),
            tick_rate: default_tick_rate(),
            snapshot_rate: default_snapshot_rate(),
            max_clients: default_max_clients(),
            handshake_timeout: default_handshake_timeout(),
            chat_min_interval: default_chat_interval(),
            auto_start: false,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ProjectSerial {
    #[serde(rename = "Version", default = "default_version")]
    pub version: String,
    #[serde(rename = "Name", default = "default_name")]
    pub name: String,
    #[serde(rename = "AssetPath", default = "default_asset_path")]
    pub asset_path: String,
    #[serde(rename = "AssetRegistry", default = "default_asset_reg")]
    pub asset_registry: String,
    #[serde(rename = "DefaultSceneHandle", default)]
    pub default_scene_handle: u64,
    #[serde(rename = "Configuration", default = "default_config")]
    pub configuration: String,
    #[serde(rename = "Physics", skip_serializing_if = "Option::is_none", default)]
    pub physics: Option<PhysicsConfigSerial>,
    #[serde(rename = "Server", skip_serializing_if = "Option::is_none", default)]
    pub server: Option<ServerConfigSerial>,
}

impl Default for ProjectSerial {
    fn default() -> Self {
        Self {
            version: default_version(),
            name: default_name(),
            asset_path: default_asset_path(),
            asset_registry: default_asset_reg(),
            default_scene_handle: 0,
            configuration: default_config(),
            physics: Some(PhysicsConfigSerial::default()),
            server: Some(ServerConfigSerial::default()),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Default)]
pub struct ProjectFile {
    #[serde(rename = "Project")]
    pub project: ProjectSerial,
}

impl ProjectFile {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            project: ProjectSerial {
                name: name.into(),
                ..Default::default()
            },
        }
    }

    pub fn from_json(json_str: &str) -> Result<Self, serde_json::Error> {
        serde_json::from_str(json_str)
    }

    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(self)
    }

    pub fn from_yaml(yaml_str: &str) -> Result<Self, serde_yaml::Error> {
        serde_yaml::from_str(yaml_str)
    }

    pub fn to_yaml(&self) -> Result<String, serde_yaml::Error> {
        serde_yaml::to_string(self)
    }

    pub fn from_file(filepath: impl AsRef<Path>) -> Result<Self, Box<dyn std::error::Error>> {
        let content = fs::read_to_string(filepath)?;
        // Try parsing JSON first, then YAML
        if let Ok(file) = Self::from_json(&content) {
            return Ok(file);
        }
        let file = Self::from_yaml(&content)?;
        Ok(file)
    }

    pub fn save_file(&self, filepath: impl AsRef<Path>) -> Result<(), Box<dyn std::error::Error>> {
        let content = self.to_json()?;
        if let Some(parent) = filepath.as_ref().parent() {
            fs::create_dir_all(parent).ok();
        }
        fs::write(filepath, content)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_project_serialization() {
        let file = ProjectFile::new("MyGame");
        let json_str = file.to_json().unwrap();
        assert!(json_str.contains("\"Name\": \"MyGame\""));
        assert!(json_str.contains("\"Server\""));
        assert!(json_str.contains("\"Port\": 8192"));
        assert!(json_str.contains("\"Physics\""));

        let loaded = ProjectFile::from_json(&json_str).unwrap();
        assert_eq!(loaded, file);
    }

    #[test]
    fn test_custom_server_config() {
        let mut file = ProjectFile::new("MultiplayerArena");
        file.project.server = Some(ServerConfigSerial {
            port: 7777,
            tick_rate: 120,
            snapshot_rate: 30,
            max_clients: 64,
            handshake_timeout: 10.0,
            chat_min_interval: 0.1,
            auto_start: true,
        });

        let json_str = file.to_json().unwrap();
        let loaded = ProjectFile::from_json(&json_str).unwrap();
        let s = loaded.project.server.unwrap();
        assert_eq!(s.port, 7777);
        assert_eq!(s.tick_rate, 120);
        assert_eq!(s.snapshot_rate, 30);
        assert_eq!(s.max_clients, 64);
        assert_eq!(s.handshake_timeout, 10.0);
        assert_eq!(s.chat_min_interval, 0.1);
        assert!(s.auto_start);
    }

    #[test]
    fn test_backward_compatibility_missing_server() {
        let legacy_json = r#"{
            "Project": {
                "Version": "0.0.0",
                "Name": "LegacyProject",
                "AssetPath": "Assets",
                "AssetRegistry": "AssetRegistry.ixreg",
                "DefaultSceneHandle": 42,
                "Configuration": "Release"
            }
        }"#;

        let loaded = ProjectFile::from_json(legacy_json).unwrap();
        assert_eq!(loaded.project.name, "LegacyProject");
        assert_eq!(loaded.project.configuration, "Release");
        assert_eq!(loaded.project.default_scene_handle, 42);
        assert!(loaded.project.server.is_none());
    }
}
