// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::HashMap;
use crate::asset::{AssetHandle};
use crate::asset::asset::{AssetMetaData};

pub type AssetRegistry = HashMap<AssetHandle, AssetMetaData>;

#[repr(C)]
#[derive(Debug)]
pub struct AssetManager {
    pub asset_registry: AssetRegistry,
}

impl AssetManager {
    pub fn new() -> Self {
        let asset_registry = AssetRegistry::new();
        Self { asset_registry }
    }
}

impl Drop for AssetManager {
    fn drop(&mut self) {
        self.asset_registry.clear();
    }
}
