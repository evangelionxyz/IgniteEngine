// Copyright (c) 2026 Evangelion Manuhutu

use std::cell::RefCell;
use std::os::raw::c_char;

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IgniteResult {
    Ok = 0,
    ErrNullPointer = 1,
    ErrInvalidHandle = 2,
    ErrInvalidParam = 3,
    ErrNotFound = 4,
    ErrAlreadyExists = 5,
    ErrOperationFailed = 6,
    ErrNotInitialized = 7,
    ErrUnknown = 99,
}

// Thread-local storage for the last error message.
// Each thread gets its own error string, avoiding mutex contention.
thread_local! {
    static LAST_ERROR: RefCell<String> = RefCell::new(String::new());
}

/// Sets the last error message for the current thread.
/// Call this from any FFI function before returning an error result.
pub fn set_last_error(msg: impl Into<String>) {
    LAST_ERROR.with(|e| {
        *e.borrow_mut() = msg.into();
    });
}

/// Clears the last error message for the current thread.
pub fn clear_last_error() {
    LAST_ERROR.with(|e| {
        e.borrow_mut().clear();
    });
}

/// Returns a pointer to the last error message as a null-terminated C string.
/// The pointer is valid until the next FFI call on the same thread.
/// Returns an empty string if no error has been set.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_get_last_error(out_buf: *mut c_char, max_len: usize) -> usize {
    if out_buf.is_null() || max_len == 0 {
        return 0;
    }
    LAST_ERROR.with(|e| {
        let msg = e.borrow();
        let bytes = msg.as_bytes();
        let copy_len = bytes.len().min(max_len - 1);
        unsafe {
            std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
            *out_buf.add(copy_len) = 0; // null-terminate
        }
        copy_len
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_result_to_string(result: IgniteResult) -> *const c_char {
    match result {
        IgniteResult::Ok => "Ok\0".as_ptr() as *const c_char,
        IgniteResult::ErrNullPointer => "ErrNullPointer\0".as_ptr() as *const c_char,
        IgniteResult::ErrInvalidHandle => "ErrInvalidHandle\0".as_ptr() as *const c_char,
        IgniteResult::ErrInvalidParam => "ErrInvalidParam\0".as_ptr() as *const c_char,
        IgniteResult::ErrNotFound => "ErrNotFound\0".as_ptr() as *const c_char,
        IgniteResult::ErrAlreadyExists => "ErrAlreadyExists\0".as_ptr() as *const c_char,
        IgniteResult::ErrOperationFailed => "ErrOperationFailed\0".as_ptr() as *const c_char,
        IgniteResult::ErrNotInitialized => "ErrNotInitialized\0".as_ptr() as *const c_char,
        IgniteResult::ErrUnknown => "ErrUnknown\0".as_ptr() as *const c_char,
    }
}
