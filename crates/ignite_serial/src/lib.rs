// Copyright (c) 2026 Evangelion Manuhutu

use serde::{Serialize, Deserialize};
use ignite_asset::{AssetHandle, AssetType};

pub fn serialize_yaml<T: Serialize>(value: &T) -> Result<String, serde_yaml::Error> {
    serde_yaml::to_string(value)
}

pub fn deserialize_yaml<T: for<'de> Deserialize<'de>>(yaml_str: &str) -> Result<T, serde_yaml::Error> {
    serde_yaml::from_str(yaml_str)
}

pub fn serialize_binary<T: Serialize>(value: &T) -> Result<Vec<u8>, bincode::Error> {
    bincode::serialize(value)
}

pub fn deserialize_binary<T: for<'de> Deserialize<'de>>(bytes: &[u8]) -> Result<T, bincode::Error> {
    bincode::deserialize(bytes)
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct AssetRegistryItem {
    pub handle: AssetHandle,
    pub filepath: String,
    pub asset_type: AssetType,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq, Default)]
pub struct AssetRegistryFile {
    pub assets: Vec<AssetRegistryItem>,
}

impl AssetRegistryFile {
    pub fn new() -> Self {
        Self::default()
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
        file.assets.push(AssetRegistryItem {
            handle: AssetHandle::from_u64(1001),
            filepath: "Assets/Textures/ground.png".to_string(),
            asset_type: AssetType::Texture,
        });
        file.assets.push(AssetRegistryItem {
            handle: AssetHandle::from_u64(1002),
            filepath: "Assets/Scenes/Main.ixscene".to_string(),
            asset_type: AssetType::Scene,
        });

        let yaml_str = file.to_yaml().unwrap();
        let loaded = AssetRegistryFile::from_yaml(&yaml_str).unwrap();
        assert_eq!(loaded, file);
    }
}
