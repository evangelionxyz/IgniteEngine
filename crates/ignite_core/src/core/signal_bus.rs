// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::HashMap;
use std::sync::Mutex;

pub type SignalHandler = Box<dyn Fn(&[u8]) + Send + Sync>;

#[derive(Default)]
pub struct SignalBus {
    signals: HashMap<String, Vec<SignalHandler>>,
}

impl SignalBus {
    pub fn new() -> Self {
        Self {
            signals: HashMap::new(),
        }
    }

    pub fn subscribe<F>(&mut self, signal_name: &str, handler: F)
    where
        F: Fn(&[u8]) + Send + Sync + 'static,
    {
        self.signals
            .entry(signal_name.to_string())
            .or_default()
            .push(Box::new(handler));
    }

    pub fn publish(&self, signal_name: &str, payload: &[u8]) -> usize {
        if let Some(handlers) = self.signals.get(signal_name) {
            for handler in handlers {
                handler(payload);
            }
            handlers.len()
        } else {
            0
        }
    }

    pub fn clear(&mut self) {
        self.signals.clear();
    }
}

static GLOBAL_SIGNAL_BUS: Mutex<Option<SignalBus>> = Mutex::new(None);

pub fn global_signal_bus_publish(signal_name: &str, payload: &[u8]) -> usize {
    let lock = match GLOBAL_SIGNAL_BUS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(bus) = lock.as_ref() {
        bus.publish(signal_name, payload)
    } else {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};
    use std::sync::Arc;

    #[test]
    fn test_signal_bus() {
        let mut bus = SignalBus::new();
        let counter = Arc::new(AtomicU32::new(0));

        let c_clone = counter.clone();
        bus.subscribe("on_event", move |_payload| {
            c_clone.fetch_add(1, Ordering::SeqCst);
        });

        let count = bus.publish("on_event", &[]);
        assert_eq!(count, 1);
        assert_eq!(counter.load(Ordering::SeqCst), 1);
    }
}
