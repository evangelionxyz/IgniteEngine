// Copyright (c) 2026 Evangelion Manuhutu

use crate::asset::AssetManager;
use std::sync::Mutex;

#[repr(C)]
pub struct IgniteEngine {
    pub asset_manager: AssetManager,
}

impl IgniteEngine {
    pub fn new() -> Self {
        Self {
            asset_manager: AssetManager::new(),
        }
    }
}

impl Default for IgniteEngine {
    fn default() -> Self {
        Self::new()
    }
}

static ENGINE_INSTANCE: Mutex<Option<IgniteEngine>> = Mutex::new(None);

pub fn init_engine() -> bool {
    let mut lock = match ENGINE_INSTANCE.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    if lock.is_none() {
        *lock = Some(IgniteEngine::new());
        true
    } else {
        false
    }
}

pub fn shutdown_engine() -> bool {
    let mut lock = match ENGINE_INSTANCE.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    if lock.is_some() {
        *lock = None;
        true
    } else {
        false
    }
}

pub fn is_engine_initialized() -> bool {
    let lock = match ENGINE_INSTANCE.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    lock.is_some()
}

