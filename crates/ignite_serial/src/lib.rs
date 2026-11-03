// Copyright (c) 2026 Evangelion Manuhutu

pub mod scene;
pub mod project;

pub use project::*;

use serde::{Serialize, Deserialize};
use ignite_asset::{AssetHandle, AssetType};

// JSON Serializer
pub fn serialize_json<T: Serialize>(value: &T) -> Result<String, serde_json::Error> {
    serde_json::to_string(value)
}

pub fn deserialize_json<T: for<'de> Deserialize<'de>>(json_str: &str) -> Result<T, serde_json::Error> {
    serde_json::from_str(json_str)
}

// YAML Serializer
pub fn serialize_yaml<T: Serialize>(value: &T) -> Result<String, serde_yaml::Error> {
    serde_yaml::to_string(value)
}

pub fn deserialize_yaml<T: for<'de> Deserialize<'de>>(yaml_str: &str) -> Result<T, serde_yaml::Error> {
    serde_yaml::from_str(yaml_str)
}

// BINARY Serializer
pub fn serialize_binary<T: Serialize>(value: &T) -> Result<Vec<u8>, bincode::Error> {
    bincode::serialize(value)
}

pub fn deserialize_binary<T: for<'de> Deserialize<'de>>(bytes: &[u8]) -> Result<T, bincode::Error> {
    bincode::deserialize(bytes)
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetRegistryItem {
    #[serde(rename = "Handle")]
    pub handle: AssetHandle,
    #[serde(rename = "Type")]
    pub asset_type: AssetType,
    #[serde(rename = "Filepath")]
    pub filepath: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Default)]
pub struct AssetRegistryWrapper {
    #[serde(rename = "Assets")]
    pub assets: Vec<AssetRegistryItem>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Default)]
pub struct AssetRegistryFile {
    #[serde(rename = "AssetRegistry")]
    pub registry: AssetRegistryWrapper,
}

// Asset Registry File
impl AssetRegistryFile {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serialize_json(self)
    }

    pub fn from_json(json_str: &str) -> Result<Self, serde_json::Error> {
        deserialize_json(json_str)
    }

    pub fn to_yaml(&self) -> Result<String, serde_yaml::Error> {
        serialize_yaml(self)
    }

    pub fn from_yaml(yaml_str: &str) -> Result<Self, serde_yaml::Error> {
        deserialize_yaml(yaml_str)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ignite_asset::AssetMetaData;

    #[test]
    fn test_yaml_serialization() {
        let meta = AssetMetaData::new("Assets/Textures/character.png", AssetType::Texture);
        let yaml_str = serialize_yaml(&meta).unwrap();
        assert!(yaml_str.contains("filepath"));
        assert!(yaml_str.contains("Texture"));

        let loaded: AssetMetaData = deserialize_yaml(&yaml_str).unwrap();
        assert_eq!(loaded, meta);
    }

    #[test]
    fn test_binary_serialization() {
        let meta = AssetMetaData::new("Assets/Meshes/hero.mesh", AssetType::Mesh);
        let bytes = serialize_binary(&meta).unwrap();
        assert!(!bytes.is_empty());

        let loaded: AssetMetaData = deserialize_binary(&bytes).unwrap();
        assert_eq!(loaded, meta);
    }

    #[test]
    fn test_asset_registry_file_yaml() {
        let mut file = AssetRegistryFile::new();
        file.registry.assets.push(AssetRegistryItem {
            handle: AssetHandle::from_u64(1001),
            filepath: "Assets/Textures/ground.png".to_string(),
            asset_type: AssetType::Texture,
        });
        file.registry.assets.push(AssetRegistryItem {
            handle: AssetHandle::from_u64(1002),
            filepath: "Assets/Scenes/Main.ixscene".to_string(),
            asset_type: AssetType::Scene,
        });

        let yaml_str = file.to_yaml().unwrap();
        let loaded = AssetRegistryFile::from_yaml(&yaml_str).unwrap();
        assert_eq!(loaded, file);
    }

    #[test]
    fn test_fps_game_asset_registry_sample() {
        let sample_yaml = r#"
AssetRegistry:
  Assets:
    - Handle: 42972961298889726
      Type: Material
      Filepath: Assets/StaticMeshes/Sponza/Material_18.ixmat
    - Handle: 1801218881103323832
      Type: StaticMesh
      Filepath: Assets/StaticMeshes/Sponza/Sponza.mesh
"#;
        let loaded = AssetRegistryFile::from_yaml(sample_yaml).unwrap();
        assert_eq!(loaded.registry.assets.len(), 2);
        assert_eq!(loaded.registry.assets[0].handle, AssetHandle::from_u64(42972961298889726));
        assert_eq!(loaded.registry.assets[0].asset_type, AssetType::Material);
        assert_eq!(loaded.registry.assets[0].filepath, "Assets/StaticMeshes/Sponza/Material_18.ixmat");

        assert_eq!(loaded.registry.assets[1].handle, AssetHandle::from_u64(1801218881103323832));
        assert_eq!(loaded.registry.assets[1].asset_type, AssetType::StaticMesh);
        assert_eq!(loaded.registry.assets[1].filepath, "Assets/StaticMeshes/Sponza/Sponza.mesh");
    }
}
