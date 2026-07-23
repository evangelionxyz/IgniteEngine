// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::sync::Mutex;
use ignite_asset::{AssetHandle, AssetManager, AssetMetaData, AssetType};
use crate::ffi::result_ffi::IgniteResult;

static GLOBAL_ASSET_MANAGER: Mutex<Option<AssetManager>> = Mutex::new(None);

fn with_asset_manager<F, R>(f: F) -> R
where
    F: FnOnce(&mut AssetManager) -> R,
{
    let mut lock = match GLOBAL_ASSET_MANAGER.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    let manager = lock.get_or_insert_with(AssetManager::new);
    f(manager)
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_assign_metadata(
    handle: u64,
    path_ptr: *const c_char,
    asset_type: AssetType,
) -> IgniteResult {
    if handle == 0 || path_ptr.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let path_str = match CStr::from_ptr(path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };
        let meta = AssetMetaData::new(path_str, asset_type);
        with_asset_manager(|mgr| {
            if mgr.assign_metadata(AssetHandle::from_u64(handle), meta) {
                IgniteResult::Ok
            } else {
                IgniteResult::ErrInvalidHandle
            }
        })
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_get_metadata(
    handle: u64,
    out_path_buf: *mut c_char,
    max_len: usize,
    out_type: *mut AssetType,
) -> IgniteResult {
    if handle == 0 || out_path_buf.is_null() || out_type.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    with_asset_manager(|mgr| {
        if let Some(meta) = mgr.get_metadata(AssetHandle::from_u64(handle)) {
            unsafe {
                *out_type = meta.asset_type;
                let bytes = meta.filepath.as_bytes();
                let copy_len = bytes.len().min(max_len - 1);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_path_buf, copy_len);
                *out_path_buf.add(copy_len) = 0; // null terminate
            }
            IgniteResult::Ok
        } else {
            IgniteResult::ErrNotFound
        }
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_remove_metadata(handle: u64) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    with_asset_manager(|mgr| {
        if mgr.remove_metadata(AssetHandle::from_u64(handle)) {
            IgniteResult::Ok
        } else {
            IgniteResult::ErrNotFound
        }
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_pin(handle: u64) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle; // Rule 13: AssetHandle(0) is invalid, never pin
    }
    with_asset_manager(|mgr| {
        if mgr.pin_asset(AssetHandle::from_u64(handle)) {
            IgniteResult::Ok
        } else {
            IgniteResult::ErrInvalidHandle
        }
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_unpin(handle: u64) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    with_asset_manager(|mgr| {
        if mgr.unpin_asset(AssetHandle::from_u64(handle)) {
            IgniteResult::Ok
        } else {
            IgniteResult::ErrNotFound
        }
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_is_pinned(handle: u64) -> bool {
    if handle == 0 {
        return false;
    }
    with_asset_manager(|mgr| mgr.is_pinned(AssetHandle::from_u64(handle)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_get_pin_count(handle: u64) -> u32 {
    if handle == 0 {
        return 0;
    }
    with_asset_manager(|mgr| mgr.get_pin_count(AssetHandle::from_u64(handle)))
}
