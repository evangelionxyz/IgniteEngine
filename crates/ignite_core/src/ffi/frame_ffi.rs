// Copyright (c) 2026 Evangelion Manuhutu

use crate::engine::with_engine;
use crate::engine::with_engine_ref;
use crate::ffi::result_ffi::{IgniteResult, set_last_error, clear_last_error};

/// Called by C++ at the beginning of each frame.
/// Updates Rust-side timing, increments frame counter.
///
/// # Arguments
/// * `delta_time` - Time elapsed since last frame, in seconds
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_engine_begin_frame(delta_time: f32) -> IgniteResult {
    clear_last_error();
    match with_engine(|engine| {
        engine.frame.begin_frame(delta_time);
    }) {
        Some(_) => IgniteResult::Ok,
        None => {
            set_last_error("Engine not initialized: cannot begin frame");
            IgniteResult::ErrNotInitialized
        }
    }
}

/// Called by C++ at the end of each frame.
/// Flushes all deferred callbacks queued during the frame.
///
/// # Returns
/// * `IgniteResult::Ok` on success
/// * `IgniteResult::ErrNotInitialized` if engine is not initialized
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_engine_end_frame() -> IgniteResult {
    clear_last_error();
    match with_engine(|engine| {
        engine.frame.end_frame();
    }) {
        Some(_) => IgniteResult::Ok,
        None => {
            set_last_error("Engine not initialized: cannot end frame");
            IgniteResult::ErrNotInitialized
        }
    }
}

/// Returns the current frame count (number of frames since engine init).
/// Returns 0 if the engine is not initialized.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_get_frame_count() -> u64 {
    with_engine_ref(|engine| engine.frame.frame_count).unwrap_or(0)
}

/// Returns the total accumulated time since engine init, in seconds.
/// Returns 0.0 if the engine is not initialized.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_get_total_time() -> f64 {
    with_engine_ref(|engine| engine.frame.total_time).unwrap_or(0.0)
}

/// Returns the current frame's delta time, in seconds.
/// Returns 0.0 if the engine is not initialized.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_get_delta_time() -> f32 {
    with_engine_ref(|engine| engine.frame.delta_time).unwrap_or(0.0)
}

/// Returns whether the engine is currently in a frame (between begin_frame and end_frame).
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_is_in_frame() -> bool {
    with_engine_ref(|engine| engine.frame.in_frame).unwrap_or(false)
}

/// Returns the number of pending deferred callbacks.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_get_pending_deferred_count() -> usize {
    with_engine_ref(|engine| engine.frame.pending_deferred_count()).unwrap_or(0)
}
