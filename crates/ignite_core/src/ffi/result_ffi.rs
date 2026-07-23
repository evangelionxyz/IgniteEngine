// Copyright (c) 2026 Evangelion Manuhutu

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
    ErrUnknown = 99,
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
        IgniteResult::ErrUnknown => "ErrUnknown\0".as_ptr() as *const c_char,
    }
}
