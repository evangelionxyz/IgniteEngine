// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::HashMap;
use crate::{AssetHandle, AssetMetaData};

pub type AssetRegistry = HashMap<AssetHandle, AssetMetaData>;

#[repr(C)]
#[derive(Debug, Default)]
pub struct AssetManager {
    pub asset_registry: AssetRegistry,
    pub pinned_assets: HashMap<AssetHandle, u32>,
}

impl AssetManager {
    pub fn new() -> Self {
        Self {
            asset_registry: AssetRegistry::new(),
            pinned_assets: HashMap::new(),
        }
    }

    pub fn assign_metadata(&mut self, handle: AssetHandle, metadata: AssetMetaData) -> bool {
        if !handle.is_valid() {
            return false;
        }
        self.asset_registry.insert(handle, metadata);
        true
    }

    pub fn get_metadata(&self, handle: AssetHandle) -> Option<&AssetMetaData> {
        if !handle.is_valid() {
            return None;
        }
        self.asset_registry.get(&handle)
    }

    pub fn remove_metadata(&mut self, handle: AssetHandle) -> bool {
        if !handle.is_valid() {
            return false;
        }
        self.pinned_assets.remove(&handle);
        self.asset_registry.remove(&handle).is_some()
    }

    pub fn pin_asset(&mut self, handle: AssetHandle) -> bool {
        if !handle.is_valid() {
            return false; // AssetHandle(0) must NOT be pinned
        }
        let count = self.pinned_assets.entry(handle).or_insert(0);
        *count += 1;
        true
    }

    pub fn unpin_asset(&mut self, handle: AssetHandle) -> bool {
        if !handle.is_valid() {
            return false;
        }
        if let std::collections::hash_map::Entry::Occupied(mut entry) = self.pinned_assets.entry(handle) {
            let count = entry.get_mut();
            if *count > 1 {
                *count -= 1;
            } else {
                entry.remove();
            }
            true
        } else {
            false
        }
    }

    pub fn is_pinned(&self, handle: AssetHandle) -> bool {
        if !handle.is_valid() {
            return false;
        }
        self.pinned_assets.contains_key(&handle)
    }

    pub fn get_pin_count(&self, handle: AssetHandle) -> u32 {
        if !handle.is_valid() {
            return 0;
        }
        self.pinned_assets.get(&handle).copied().unwrap_or(0)
    }

    pub fn clear(&mut self) {
        self.pinned_assets.clear();
        self.asset_registry.clear();
    }
}

impl Drop for AssetManager {
    fn drop(&mut self) {
        self.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::AssetType;

    #[test]
    fn test_asset_pinning() {
        let mut manager = AssetManager::new();
        let invalid = AssetHandle::from_u64(0);
        let valid = AssetHandle::from_u64(101);

        assert!(!manager.pin_asset(invalid));
        assert_eq!(manager.get_pin_count(invalid), 0);

        assert!(manager.pin_asset(valid));
        assert!(manager.is_pinned(valid));
        assert_eq!(manager.get_pin_count(valid), 1);

        assert!(manager.pin_asset(valid));
        assert_eq!(manager.get_pin_count(valid), 2);

        assert!(manager.unpin_asset(valid));
        assert_eq!(manager.get_pin_count(valid), 1);

        assert!(manager.unpin_asset(valid));
        assert!(!manager.is_pinned(valid));
        assert_eq!(manager.get_pin_count(valid), 0);
    }

    #[test]
    fn test_metadata_registry() {
        let mut manager = AssetManager::new();
        let handle = AssetHandle::from_u64(202);
        let meta = AssetMetaData::new("Assets/Textures/logo.png", AssetType::Texture);

        assert!(manager.assign_metadata(handle, meta.clone()));
        assert_eq!(manager.get_metadata(handle), Some(&meta));

        assert!(manager.remove_metadata(handle));
        assert_eq!(manager.get_metadata(handle), None);
    }
}
