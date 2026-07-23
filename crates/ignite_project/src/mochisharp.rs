// Copyright (c) 2026 Evangelion Manuhutu

use std::sync::Mutex;

pub type MochiSharpTickCallback = extern "C" fn(delta_time: f32);

static SCRIPT_TICK_CALLBACK: Mutex<Option<MochiSharpTickCallback>> = Mutex::new(None);

pub fn register_script_tick_callback(cb: MochiSharpTickCallback) {
    let mut lock = match SCRIPT_TICK_CALLBACK.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    *lock = Some(cb);
}

pub fn unregister_script_tick_callback() {
    let mut lock = match SCRIPT_TICK_CALLBACK.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    *lock = None;
}

pub fn trigger_script_tick(delta_time: f32) -> bool {
    let lock = match SCRIPT_TICK_CALLBACK.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(cb) = *lock {
        cb(delta_time);
        true
    } else {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};

    static TICK_COUNT: AtomicU32 = AtomicU32::new(0);

    extern "C" fn mock_tick(_dt: f32) {
        TICK_COUNT.fetch_add(1, Ordering::Relaxed);
    }

    #[test]
    fn test_mochisharp_bridge() {
        register_script_tick_callback(mock_tick);
        assert!(trigger_script_tick(0.016));
        assert_eq!(TICK_COUNT.load(Ordering::Relaxed), 1);

        unregister_script_tick_callback();
        assert!(!trigger_script_tick(0.016));
    }
}
