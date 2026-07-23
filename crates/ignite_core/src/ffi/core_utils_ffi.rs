// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use crate::core::string_utils as su;
use crate::core::hashing as hash_mod;
use crate::core::time as time_mod;
use crate::core::signal_bus as sig_mod;
use crate::ffi::result_ffi::IgniteResult;

// -------------------------------------------------
// String Utilities FFI
// -------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_string_ends_with(str_ptr: *const c_char, ending_ptr: *const c_char) -> bool {
    if str_ptr.is_null() || ending_ptr.is_null() {
        return false;
    }
    unsafe {
        let s = match CStr::from_ptr(str_ptr).to_str() {
            Ok(val) => val,
            Err(_) => return false,
        };
        let e = match CStr::from_ptr(ending_ptr).to_str() {
            Ok(val) => val,
            Err(_) => return false,
        };
        su::ends_with(s, e)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_string_to_lower(
    str_ptr: *const c_char,
    out_buf: *mut c_char,
    max_len: usize,
) -> usize {
    if str_ptr.is_null() || out_buf.is_null() || max_len == 0 {
        return 0;
    }
    unsafe {
        let s = match CStr::from_ptr(str_ptr).to_str() {
            Ok(val) => val,
            Err(_) => return 0,
        };
        let lower = su::to_lower(s);
        let bytes = lower.as_bytes();
        let copy_len = bytes.len().min(max_len - 1);
        std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
        *out_buf.add(copy_len) = 0; // null-terminate
        copy_len
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_string_trim(
    str_ptr: *const c_char,
    out_buf: *mut c_char,
    max_len: usize,
) -> usize {
    if str_ptr.is_null() || out_buf.is_null() || max_len == 0 {
        return 0;
    }
    unsafe {
        let s = match CStr::from_ptr(str_ptr).to_str() {
            Ok(val) => val,
            Err(_) => return 0,
        };
        let trimmed = su::trim(s);
        let bytes = trimmed.as_bytes();
        let copy_len = bytes.len().min(max_len - 1);
        std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
        *out_buf.add(copy_len) = 0; // null-terminate
        copy_len
    }
}

// -------------------------------------------------
// Hashing FFI
// -------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_hash_combine(seed: u64, hash_val: u64) -> u64 {
    let mut s = seed;
    hash_mod::hash_combine(&mut s, hash_val);
    s
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_hash_string(str_ptr: *const c_char) -> u64 {
    if str_ptr.is_null() {
        return 0;
    }
    unsafe {
        let s = match CStr::from_ptr(str_ptr).to_str() {
            Ok(val) => val,
            Err(_) => return 0,
        };
        hash_mod::hash_str(s)
    }
}

// -------------------------------------------------
// Time FFI
// -------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_timer_create() -> u64 {
    time_mod::create_timer()
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_timer_reset(id: u64) -> bool {
    time_mod::reset_timer(id)
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_timer_elapsed_seconds(id: u64) -> f32 {
    time_mod::elapsed_timer_seconds(id)
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_timer_destroy(id: u64) -> bool {
    time_mod::destroy_timer(id)
}

// -------------------------------------------------
// SignalBus FFI
// -------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_signal_publish(
    signal_name: *const c_char,
    payload_ptr: *const u8,
    payload_len: usize,
) -> IgniteResult {
    if signal_name.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let name = match CStr::from_ptr(signal_name).to_str() {
            Ok(val) => val,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };
        let payload = if !payload_ptr.is_null() && payload_len > 0 {
            std::slice::from_raw_parts(payload_ptr, payload_len)
        } else {
            &[]
        };
        sig_mod::global_signal_bus_publish(name, payload);
    }
    IgniteResult::Ok
}
