// Copyright (c) 2026 Evangelion Manuhutu

use std::fmt;
use crate::{AssetHandle, asset::AssetType};

// Asset
#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Asset {
    pub handle: AssetHandle,
    pub asset_type: AssetType,
    
    pub is_ready: bool,
    pub is_dirty: bool,
}

impl Asset {
    pub fn new(handle: AssetHandle, asset_type: AssetType) -> Self {
        Self{handle, asset_type, ..Default::default()}
    }
}

impl Default for Asset {
    fn default() -> Self {
        Self{ 
            handle: AssetHandle::NULL,
            asset_type: AssetType::Invalid,
            is_ready: true,
            is_dirty: false
        }
    }
}

impl fmt::Display for Asset {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "type: {} handle: {}", self.asset_type, self.handle)
    }
}

// MetaData
#[repr(C)]
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct AssetMetaData {
    pub filepath: String,
    pub asset_type: AssetType,
}

