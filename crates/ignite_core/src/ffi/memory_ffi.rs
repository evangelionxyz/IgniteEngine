// Copyright (c) 2026 Evangelion Manuhutu

use std::collections::HashMap;
use std::sync::Mutex;
use crate::ffi::result_ffi::IgniteResult;

static ALLOCATED_BUFFERS: Mutex<Option<HashMap<u64, Vec<u8>>>> = Mutex::new(None);
static NEXT_BUFFER_HANDLE: Mutex<u64> = Mutex::new(1);

/// Test helper for memory boundary verification:
/// Rust allocates a CPU buffer and stores it, returning an opaque handle and raw data pointer to C++.
/// C++ accesses the data but NEVER frees the pointer directly. C++ must call `ignite_test_free_buffer(handle)`.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_test_alloc_buffer(
    size: usize,
    fill_byte: u8,
    out_handle: *mut u64,
    out_ptr: *mut *const u8,
) -> IgniteResult {
    if out_handle.is_null() || out_ptr.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    if size == 0 {
        return IgniteResult::ErrInvalidParam;
    }

    let buffer = vec![fill_byte; size];
    let ptr = buffer.as_ptr();

    let mut next_id = match NEXT_BUFFER_HANDLE.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    let handle = *next_id;
    *next_id += 1;

    let mut lock = match ALLOCATED_BUFFERS.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    let map = lock.get_or_insert_with(HashMap::new);
    map.insert(handle, buffer);

    unsafe {
        *out_handle = handle;
        *out_ptr = ptr;
    }

    IgniteResult::Ok
}

/// Frees a buffer allocated via `ignite_test_alloc_buffer` by handle.
#[unsafe(no_mangle)]
pub extern "C" fn ignite_test_free_buffer(handle: u64) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }

    let mut lock = match ALLOCATED_BUFFERS.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };

    if let Some(map) = lock.as_mut() {
        if map.remove(&handle).is_some() {
            return IgniteResult::Ok;
        }
    }

    IgniteResult::ErrNotFound
}
