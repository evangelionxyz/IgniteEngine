// Copyright (c) 2026 Evangelion Manuhutu

use ignite_asset::AssetManager;
use std::sync::{Mutex, OnceLock};

use crate::frame::FrameContext;
use crate::{IgniteLogLevel, log_internal};

/// Engine version encoded as (major << 22 | minor << 12 | patch)
/// matching the C++ IGN_MAKE_VERSION macro.
pub const ENGINE_VERSION_MAJOR: u32 = 0;
pub const ENGINE_VERSION_MINOR: u32 = 1;
pub const ENGINE_VERSION_PATCH: u32 = 0;

pub const fn make_version(major: u32, minor: u32, patch: u32) -> u32 {
    (major << 22) | (minor << 12) | patch
}

pub const ENGINE_VERSION: u32 = make_version(ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_PATCH);

pub struct IgniteEngine {
    pub asset_manager: AssetManager,
    pub frame: FrameContext,
}

impl IgniteEngine {
    pub fn new() -> Self {
        Self {
            asset_manager: AssetManager::new(),
            frame: FrameContext::new(),
        }
    }
}

impl Default for IgniteEngine {
    fn default() -> Self {
        Self::new()
    }
}

/// Engine singleton using OnceLock for initialization + Mutex for interior mutability.
/// OnceLock ensures the Mutex is only created once and avoids the Option wrapper.
static ENGINE_INSTANCE: OnceLock<Mutex<Option<IgniteEngine>>> = OnceLock::new();

fn get_engine_mutex() -> &'static Mutex<Option<IgniteEngine>> {
    ENGINE_INSTANCE.get_or_init(|| Mutex::new(None))
}

/// Safe accessor: runs a closure with mutable access to the engine.
/// Returns None if the engine is not initialized.
pub fn with_engine<F, R>(f: F) -> Option<R>
where
    F: FnOnce(&mut IgniteEngine) -> R,
{
    let mut lock = match get_engine_mutex().lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    lock.as_mut().map(f)
}

/// Safe accessor: runs a closure with immutable access to the engine.
/// Returns None if the engine is not initialized.
pub fn with_engine_ref<F, R>(f: F) -> Option<R>
where
    F: FnOnce(&IgniteEngine) -> R,
{
    let lock = match get_engine_mutex().lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    lock.as_ref().map(f)
}

pub fn init_engine() -> bool {
    let mut lock = match get_engine_mutex().lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    if lock.is_none() {
        *lock = Some(IgniteEngine::new());
        log_internal(IgniteLogLevel::Warn, &format!(
            "Rust engine initialized (v{}.{}.{})",
            ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_PATCH
        ));
        true
    } else {
        log_internal(IgniteLogLevel::Error, "Failed to initialize Rust engine: already initialized");
        false
    }
}

pub fn shutdown_engine() -> bool {
    let mut lock = match get_engine_mutex().lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    if lock.is_some() {
        *lock = None;
        log_internal(IgniteLogLevel::Warn, "Rust engine shutdown");
        true
    } else {
        false
    }
}

pub fn is_engine_initialized() -> bool {
    let lock = match get_engine_mutex().lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    lock.is_some()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_make_version() {
        let v = make_version(1, 2, 3);
        assert_eq!((v >> 22) & 0x3FF, 1);
        assert_eq!((v >> 12) & 0x3FF, 2);
        assert_eq!(v & 0xFFF, 3);
    }

    #[test]
    fn test_engine_version_constant() {
        assert_eq!(ENGINE_VERSION, make_version(0, 1, 0));
    }
}
