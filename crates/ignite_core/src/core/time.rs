// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::HashMap;
use std::sync::Mutex;
use std::time::Instant;

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Timestep {
    pub time: f32,
}

impl Timestep {
    pub fn new(time: f32) -> Self {
        Self { time }
    }

    pub fn seconds(&self) -> f32 {
        self.time
    }

    pub fn milliseconds(&self) -> f32 {
        self.time * 1000.0
    }
}

pub struct Timer {
    start: Instant,
}

impl Timer {
    pub fn new() -> Self {
        Self {
            start: Instant::now(),
        }
    }

    pub fn reset(&mut self) {
        self.start = Instant::now();
    }

    pub fn elapsed_seconds(&self) -> f32 {
        self.start.elapsed().as_secs_f32()
    }

    pub fn elapsed_millis(&self) -> f32 {
        self.start.elapsed().as_secs_f32() * 1000.0
    }
}

impl Default for Timer {
    fn default() -> Self {
        Self::new()
    }
}

// Timer manager for FFI interop
static TIMERS: Mutex<Option<HashMap<u64, Timer>>> = Mutex::new(None);
static NEXT_TIMER_ID: Mutex<u64> = Mutex::new(1);

pub fn create_timer() -> u64 {
    let mut next_id = match NEXT_TIMER_ID.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    let id = *next_id;
    *next_id += 1;

    let mut lock = match TIMERS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    let map = lock.get_or_insert_with(HashMap::new);
    map.insert(id, Timer::new());
    id
}

pub fn reset_timer(id: u64) -> bool {
    let mut lock = match TIMERS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        if let Some(timer) = map.get_mut(&id) {
            timer.reset();
            return true;
        }
    }
    false
}

pub fn elapsed_timer_seconds(id: u64) -> f32 {
    let lock = match TIMERS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(timer) = map.get(&id) {
            return timer.elapsed_seconds();
        }
    }
    0.0
}

pub fn destroy_timer(id: u64) -> bool {
    let mut lock = match TIMERS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        return map.remove(&id).is_some();
    }
    false
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_timestep() {
        let ts = Timestep::new(0.016);
        assert_eq!(ts.seconds(), 0.016);
        assert_eq!(ts.milliseconds(), 16.0);
    }

    #[test]
    fn test_timer() {
        let id = create_timer();
        assert!(id > 0);
        let elapsed = elapsed_timer_seconds(id);
        assert!(elapsed >= 0.0);
        assert!(reset_timer(id));
        assert!(destroy_timer(id));
    }
}
