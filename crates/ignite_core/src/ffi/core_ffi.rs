// Copyright (c) 2026 Evangelion Manuhutu

use ignite_asset::UUID;
use crate::engine::{init_engine, is_engine_initialized, shutdown_engine, ENGINE_VERSION};

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_test_connection() -> i32 {
    0x52555354 // RUST in hexadecimal
}

// Ignite Engine Lifecycle
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_engine_init() -> bool {
    init_engine()
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_engine_shutdown() -> bool {
    shutdown_engine()
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_engine_is_initialized() -> bool {
    is_engine_initialized()
}

/// Returns the Rust engine version as a packed u32 (major << 22 | minor << 12 | patch).
/// Compatible with the C++ IGN_MAKE_VERSION macro.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_engine_get_version() -> u32 {
    ENGINE_VERSION
}

// UUID
#[unsafe(no_mangle)]
pub extern "C" fn ignite_uuid_new() -> u64 {
    UUID::new().0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_uuid_from_u64(value: u64) -> u64 {
    UUID::from_u64(value).0
}

