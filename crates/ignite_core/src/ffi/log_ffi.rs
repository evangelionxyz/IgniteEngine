// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::sync::Mutex;
use crate::ffi::result_ffi::IgniteResult;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IgniteLogLevel {
    Trace = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
}

pub type LogCallbackFn = extern "C" fn(level: IgniteLogLevel, message: *const c_char);

static LOG_CALLBACK: Mutex<Option<LogCallbackFn>> = Mutex::new(None);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_log_register_callback(callback: LogCallbackFn) -> IgniteResult {
    let mut lock = match LOG_CALLBACK.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    *lock = Some(callback);
    IgniteResult::Ok
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_log_unregister_callback() -> IgniteResult {
    let mut lock = match LOG_CALLBACK.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    *lock = None;
    IgniteResult::Ok
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_log(level: IgniteLogLevel, message: *const c_char) -> IgniteResult {
    if message.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    
    let lock = match LOG_CALLBACK.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    
    if let Some(cb) = *lock {
        cb(level, message);
        IgniteResult::Ok
    } else {
        IgniteResult::ErrNotFound
    }
}

pub fn log_internal(level: IgniteLogLevel, msg: &str) {
    if let Ok(c_str) = std::ffi::CString::new(msg) {
        let _ = ignite_rs_log(level, c_str.as_ptr());
    }
}
