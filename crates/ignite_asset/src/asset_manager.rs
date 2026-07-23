// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::{HashMap, VecDeque};
use crate::{AssetHandle, AssetMetaData, AssetState};

pub type AssetRegistry = HashMap<AssetHandle, AssetMetaData>;

#[derive(Debug, Default)]
pub struct AssetManager {
    pub asset_registry: AssetRegistry,
    pub pinned_assets: HashMap<AssetHandle, u32>,

    /// Monotonically increasing version counter for registry mutations.
    /// Used by C++ to detect when a re-sync is necessary.
    pub registry_version: u64,

    /// Per-handle asset lifecycle state
    asset_states: HashMap<AssetHandle, AssetState>,

    /// Queue of handles that need to be imported by C++.
    /// C++ polls this queue each frame via `poll_import_requests()`.
    import_queue: VecDeque<AssetHandle>,
}

impl AssetManager {
    pub fn new() -> Self {
        Self {
            asset_registry: AssetRegistry::new(),
            pinned_assets: HashMap::new(),
            registry_version: 1,
            asset_states: HashMap::new(),
            import_queue: VecDeque::new(),
        }
    }

    // =========================================================================
    // Metadata Registry
    // =========================================================================

    pub fn assign_metadata(&mut self, handle: AssetHandle, metadata: AssetMetaData) -> bool {
        if !handle.is_valid() {
            return false;
        }
        self.asset_registry.insert(handle, metadata);
        
        // Initialize state to Unloaded if not already tracked
        self.asset_states.entry(handle).or_insert(AssetState::Unloaded);
        self.registry_version += 1;
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
        self.asset_states.remove(&handle);
        let removed = self.asset_registry.remove(&handle).is_some();
        if removed {
            self.registry_version += 1;
        }
        removed
    }

    pub fn registry_count(&self) -> usize {
        self.asset_registry.len()
    }

    /// Returns a snapshot of all registry entries as a Vec of (handle, type, filepath).
    /// C++ uses this to bulk-sync its local mirror of the registry.
    pub fn registry_snapshot(&self) -> Vec<(AssetHandle, AssetMetaData)> {
        self.asset_registry
            .iter()
            .map(|(&handle, meta)| (handle, meta.clone()))
            .collect()
    }

    // =========================================================================
    // Asset State Machine
    // =========================================================================

    /// Get the current state of an asset. Returns Unloaded for unknown handles.
    pub fn get_state(&self, handle: AssetHandle) -> AssetState {
        self.asset_states.get(&handle).copied().unwrap_or(AssetState::Unloaded)
    }

    /// Set the state of an asset. Returns false if the handle is not in the registry.
    pub fn set_state(&mut self, handle: AssetHandle, state: AssetState) -> bool {
        if !handle.is_valid() || !self.asset_registry.contains_key(&handle) {
            return false;
        }
        self.asset_states.insert(handle, state);
        true
    }

    /// Request that an asset be imported.
    /// Sets state to Queued and adds to the import queue.
    /// Returns false if handle is invalid or not registered.
    pub fn request_import(&mut self, handle: AssetHandle) -> bool {
        if !handle.is_valid() || !self.asset_registry.contains_key(&handle) {
            return false;
        }

        let current_state = self.get_state(handle);

        // Only queue if in Unloaded or Dirty state
        if current_state != AssetState::Unloaded && current_state != AssetState::Dirty {
            return false;
        }

        self.asset_states.insert(handle, AssetState::Queued);
        self.import_queue.push_back(handle);
        true
    }

    /// Poll the import request queue. Returns up to `max_count` handles
    /// that need importing. Each polled handle transitions to Loading state.
    pub fn poll_import_requests(&mut self, max_count: usize) -> Vec<AssetHandle> {
        let count = max_count.min(self.import_queue.len());
        let mut result = Vec::with_capacity(count);

        for _ in 0..count {
            if let Some(handle) = self.import_queue.pop_front() {
                // Only transition if still Queued (might have been cancelled)
                if self.get_state(handle) == AssetState::Queued {
                    self.asset_states.insert(handle, AssetState::Loading);
                    result.push(handle);
                }
            }
        }

        result
    }

    /// Mark an asset as Ready (called by C++ after import completes).
    pub fn mark_ready(&mut self, handle: AssetHandle) -> bool {
        if !handle.is_valid() {
            return false;
        }
        let current = self.get_state(handle);
        if current == AssetState::Loading || current == AssetState::Dirty {
            self.asset_states.insert(handle, AssetState::Ready);
            true
        } else {
            false
        }
    }

    /// Returns the number of pending import requests in the queue.
    pub fn pending_import_count(&self) -> usize {
        self.import_queue.len()
    }

    // =========================================================================
    // Asset Pinning
    // =========================================================================
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
        self.asset_states.clear();
        self.import_queue.clear();
        self.registry_version += 1;
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
        let meta = AssetMetaData::new(
            "Assets/Textures/logo.png",
            AssetType::Texture);

        assert!(manager.assign_metadata(handle, meta.clone()));
        assert_eq!(manager.get_metadata(handle), Some(&meta));

        assert!(manager.remove_metadata(handle));
        assert_eq!(manager.get_metadata(handle), None);
    }

    #[test]
    fn test_asset_state_lifecycle() {
        let mut manager = AssetManager::new();
        let handle = AssetHandle::from_u64(303);
        let meta = AssetMetaData::new(
            "Assets/Meshes/hero.mesh",
            AssetType::StaticMesh);

        // Register asset
        manager.assign_metadata(handle, meta);
        assert_eq!(manager.get_state(handle), AssetState::Unloaded);

        // Request import
        assert!(manager.request_import(handle));
        assert_eq!(manager.get_state(handle), AssetState::Queued);
        assert_eq!(manager.pending_import_count(), 1);

        // Poll — should transition to Loading
        let polled = manager.poll_import_requests(10);
        assert_eq!(polled.len(), 1);
        assert_eq!(polled[0], handle);
        assert_eq!(manager.get_state(handle), AssetState::Loading);
        assert_eq!(manager.pending_import_count(), 0);

        // Mark ready
        assert!(manager.mark_ready(handle));
        assert_eq!(manager.get_state(handle), AssetState::Ready);

        // Can't request import when already Ready
        assert!(!manager.request_import(handle));
    }

    #[test]
    fn test_registry_snapshot() {
        let mut manager = AssetManager::new();
        let h1 = AssetHandle::from_u64(100);
        let h2 = AssetHandle::from_u64(200);
        manager.assign_metadata(h1, AssetMetaData::new(
            "a.png", AssetType::Texture));
        manager.assign_metadata(h2, AssetMetaData::new(
            "b.mesh", AssetType::StaticMesh));

        let snapshot = manager.registry_snapshot();
        assert_eq!(snapshot.len(), 2);
    }

    #[test]
    fn test_import_queue_max_count() {
        let mut manager = AssetManager::new();
        let handles: Vec<_> = (1..=5).map(|i| {
            let h = AssetHandle::from_u64(i);
            manager.assign_metadata(h, AssetMetaData::new(
                format!("asset_{}.png", i), AssetType::Texture));
            manager.request_import(h);
            h
        }).collect();

        // Poll only 2
        let polled = manager.poll_import_requests(2);
        assert_eq!(polled.len(), 2);
        assert_eq!(polled[0], handles[0]);
        assert_eq!(polled[1], handles[1]);
        assert_eq!(manager.pending_import_count(), 3);

        // Poll remaining
        let polled = manager.poll_import_requests(10);
        assert_eq!(polled.len(), 3);
        assert_eq!(manager.pending_import_count(), 0);
    }
}
