// Copyright (c) 2026 Evangelion Manuhutu

use std::fmt;
use serde::{Serialize, Deserialize};
use crate::{UUID, AssetType};

pub type AssetHandle = UUID;

/// Asset lifecycle state machine.
/// Managed by Rust, observed by C++ via FFI.
///
/// State transitions:
///   Unloaded : Queued    (import requested)
///   Queued   : Loading   (C++ picks up from import queue)
///   Loading  : Ready     (C++ finishes importing)
///   Ready    : Dirty     (asset modified, needs re-save/re-import)
///   Dirty    : Ready     (re-saved)
///   Ready    : Unloading (unload requested)
///   Unloading: Unloaded  (C++ finishes cleanup)
///   Any      : Unloaded  (force unload / error)
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum AssetState {
    /// Not loaded, no pending operations
    Unloaded = 0,
    /// Import has been requested, waiting for C++ to pick up
    Queued = 1,
    /// C++ is actively importing/loading the asset
    Loading = 2,
    /// Asset is fully loaded and ready for use
    Ready = 3,
    /// Asset data has been modified, needs re-save or re-import
    Dirty = 4,
    /// Asset is being unloaded by C++
    Unloading = 5,
}

impl Default for AssetState {
    fn default() -> Self {
        AssetState::Unloaded
    }
}

impl fmt::Display for AssetState {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AssetState::Unloaded => write!(f, "Unloaded"),
            AssetState::Queued => write!(f, "Queued"),
            AssetState::Loading => write!(f, "Loading"),
            AssetState::Ready => write!(f, "Ready"),
            AssetState::Dirty => write!(f, "Dirty"),
            AssetState::Unloading => write!(f, "Unloading"),
        }
    }
}

#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Asset {
    pub handle: AssetHandle,
    pub asset_type: AssetType,
    pub state: AssetState,
    pub is_dirty: bool,
}

impl Asset {
    pub fn new(handle: AssetHandle, asset_type: AssetType) -> Self {
        Self {
            handle,
            asset_type,
            ..Default::default()
        }
    }
}

impl Default for Asset {
    fn default() -> Self {
        Self {
            handle: AssetHandle::NULL,
            asset_type: AssetType::Invalid,
            state: AssetState::Unloaded,
            is_dirty: false,
        }
    }
}

impl fmt::Display for Asset {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "type: {} handle: {} state: {}", self.asset_type, self.handle, self.state)
    }
}

#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct AssetMetaData {
    pub filepath: String,
    pub asset_type: AssetType,
}

impl AssetMetaData {
    pub fn new(filepath: impl Into<String>, asset_type: AssetType) -> Self {
        Self {
            filepath: filepath.into(),
            asset_type,
        }
    }
}

