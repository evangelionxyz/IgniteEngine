// Copyright (c) 2026 Evangelion Manuhutu

pub mod engine;
pub mod ffi;

pub use engine::*;
pub use ffi::*;

// Re-export sub-crates for convenience
pub use ignite_asset as asset;
pub use ignite_ecs as ecs;
pub use ignite_serial as serial;
pub use ignite_project as project;
