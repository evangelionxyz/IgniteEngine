// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::VecDeque;

/// Callback type for deferred work to be executed at end of frame.
pub type DeferredCallback = Box<dyn FnOnce() + Send>;

/// Per-frame context holding timing, frame count, and deferred work queues.
/// Owned by `IgniteEngine` and updated each frame via `begin_frame` / `end_frame`.
#[derive(Default)]
pub struct FrameData {
    /// Current frame's delta time in seconds
    pub delta_time: f32,
    /// Accumulated total time since engine init in seconds
    pub total_time: f64,
    /// Monotonically increasing frame counter (starts at 0)
    pub frame_count: u64,
    /// Whether we are currently between begin_frame and end_frame
    pub in_frame: bool,
    /// Queue of callbacks to run at end_frame
    deferred_queue: VecDeque<DeferredCallback>,
}

impl FrameData {
    pub fn new() -> Self {
        Self {
            delta_time: 0.0,
            total_time: 0.0,
            frame_count: 0,
            in_frame: false,
            deferred_queue: VecDeque::new(),
        }
    }

    /// Called at the start of each frame. Updates timing and increments frame counter.
    pub fn begin_frame(&mut self, delta_time: f32) {
        self.delta_time = delta_time;
        self.total_time += delta_time as f64;
        self.frame_count += 1;
        self.in_frame = true;
    }

    /// Called at the end of each frame. Flushes all deferred callbacks.
    /// Returns the number of deferred callbacks that were executed.
    pub fn end_frame(&mut self) -> usize {
        let count = self.deferred_queue.len();
        while let Some(callback) = self.deferred_queue.pop_front() {
            callback();
        }
        self.in_frame = false;
        count
    }

    /// Enqueue a callback to be executed at the next `end_frame()` call.
    pub fn defer(&mut self, callback: DeferredCallback) {
        self.deferred_queue.push_back(callback);
    }

    /// Returns the number of pending deferred callbacks.
    pub fn pending_deferred_count(&self) -> usize {
        self.deferred_queue.len()
    }

    /// Resets all frame context state (used on engine shutdown).
    pub fn reset(&mut self) {
        self.delta_time = 0.0;
        self.total_time = 0.0;
        self.frame_count = 0;
        self.in_frame = false;
        self.deferred_queue.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_frame_context_lifecycle() {
        let mut ctx = FrameData::new();
        assert_eq!(ctx.frame_count, 0);
        assert!(!ctx.in_frame);

        // First frame: 16ms
        ctx.begin_frame(0.016);
        assert!(ctx.in_frame);
        assert_eq!(ctx.frame_count, 1);
        assert!((ctx.delta_time - 0.016).abs() < f32::EPSILON);

        ctx.end_frame();
        assert!(!ctx.in_frame);

        // Second frame: 33ms
        ctx.begin_frame(0.033);
        assert_eq!(ctx.frame_count, 2);
        assert!((ctx.total_time - 0.049).abs() < 0.001);

        ctx.end_frame();
    }

    #[test]
    fn test_deferred_callbacks() {
        let mut ctx = FrameData::new();
        let counter = Arc::new(AtomicU32::new(0));

        ctx.begin_frame(0.016);

        let c1 = counter.clone();
        ctx.defer(Box::new(move || {
            c1.fetch_add(1, Ordering::SeqCst);
        }));

        let c2 = counter.clone();
        ctx.defer(Box::new(move || {
            c2.fetch_add(10, Ordering::SeqCst);
        }));

        assert_eq!(ctx.pending_deferred_count(), 2);
        let executed = ctx.end_frame();
        assert_eq!(executed, 2);
        assert_eq!(counter.load(Ordering::SeqCst), 11);
        assert_eq!(ctx.pending_deferred_count(), 0);
    }

    #[test]
    fn test_frame_context_reset() {
        let mut ctx = FrameData::new();
        ctx.begin_frame(0.016);
        ctx.begin_frame(0.016);
        ctx.defer(Box::new(|| {}));

        ctx.reset();
        assert_eq!(ctx.frame_count, 0);
        assert_eq!(ctx.total_time, 0.0);
        assert!(!ctx.in_frame);
        assert_eq!(ctx.pending_deferred_count(), 0);
    }
}
