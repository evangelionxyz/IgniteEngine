// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::sync::Mutex;
use ignite_asset::{AssetHandle, AssetManager, AssetMetaData, AssetType, AssetState};
use crate::ffi::result_ffi::IgniteResult;

static GLOBAL_ASSET_MANAGER: Mutex<Option<AssetManager>> = Mutex::new(None);

fn with_asset_manager<F, R>(f: F) -> R
where
    F: FnOnce(&mut AssetManager) -> R,
{
    if crate::engine::is_engine_initialized() {
        crate::engine::with_engine(|engine| f(&mut engine.asset_manager)).unwrap()
    } else {
        let mut lock = match GLOBAL_ASSET_MANAGER.lock() {
            Ok(g) => g,
            Err(p) => p.into_inner(),
        };
        let manager = lock.get_or_insert_with(AssetManager::new);
        f(manager)
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct IgniteAssetRegistryEntryFFI {
    pub handle: u64,
    pub asset_type: AssetType,
    pub filepath: [c_char; 260],
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
pub extern "C" fn ignite_rs_asset_get_registry_version() -> u64 {
    with_asset_manager(|mgr| mgr.registry_version)
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_get_registry_count() -> usize {
    with_asset_manager(|mgr| mgr.registry_count())
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_get_registry_snapshot(
    out_entries: *mut IgniteAssetRegistryEntryFFI,
    max_count: usize,
) -> usize {
    if out_entries.is_null() || max_count == 0 {
        return 0;
    }
    with_asset_manager(|mgr| {
        let snapshot = mgr.registry_snapshot();
        let copy_count = snapshot.len().min(max_count);
        for (i, (handle, meta)) in snapshot.into_iter().take(copy_count).enumerate() {
            unsafe {
                let entry_ptr = out_entries.add(i);
                (*entry_ptr).handle = handle.into();
                (*entry_ptr).asset_type = meta.asset_type;
                let bytes = meta.filepath.as_bytes();
                let len = bytes.len().min(259);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, (*entry_ptr).filepath.as_mut_ptr(), len);
                *(*entry_ptr).filepath.as_mut_ptr().add(len) = 0;
            }
        }
        copy_count
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_request_import(handle: u64) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    with_asset_manager(|mgr| {
        if mgr.request_import(AssetHandle::from_u64(handle)) {
            IgniteResult::Ok
        } else {
            IgniteResult::ErrOperationFailed
        }
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_poll_import_requests(
    out_handles: *mut u64,
    max_count: usize,
) -> usize {
    if out_handles.is_null() || max_count == 0 {
        return 0;
    }
    with_asset_manager(|mgr| {
        let polled = mgr.poll_import_requests(max_count);
        for (i, &handle) in polled.iter().enumerate() {
            unsafe {
                *out_handles.add(i) = handle.into();
            }
        }
        polled.len()
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_mark_ready(handle: u64) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    with_asset_manager(|mgr| {
        if mgr.mark_ready(AssetHandle::from_u64(handle)) {
            IgniteResult::Ok
        } else {
            IgniteResult::ErrOperationFailed
        }
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_get_state(handle: u64) -> AssetState {
    if handle == 0 {
        return AssetState::Unloaded;
    }
    with_asset_manager(|mgr| mgr.get_state(AssetHandle::from_u64(handle)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_asset_set_state(handle: u64, state: AssetState) -> IgniteResult {
    if handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    with_asset_manager(|mgr| {
        if mgr.set_state(AssetHandle::from_u64(handle), state) {
            IgniteResult::Ok
        } else {
            IgniteResult::ErrNotFound
        }
    })
}


