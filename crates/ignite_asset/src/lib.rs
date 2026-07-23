// Copyright (c) 2026 Evangelion Manuhutu

pub mod uuid;
pub mod asset;
pub mod asset_types;
pub mod asset_manager;

pub use uuid::*;
pub use asset::*;
pub use asset_types::*;
pub use asset_manager::*;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_uuid_generation() {
        let uuid1 = UUID::new();
        let uuid2 = UUID::new();
        assert!(uuid1.is_valid());
        assert!(uuid2.is_valid());
        assert_ne!(uuid1, uuid2);
    }

    #[test]
    fn test_asset_type_strings() {
        assert_eq!(AssetType::Texture.as_str(), "Texture");
        assert_eq!(AssetType::Scene.as_str(), "Scene");
        assert_eq!(AssetType::Prefab.as_str(), "Prefab");
    }

    #[test]
    fn test_asset_manager_creation() {
        let manager = AssetManager::new();
        assert!(manager.asset_registry.is_empty());
    }
}
