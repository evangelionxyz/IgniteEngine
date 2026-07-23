// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::collections::HashMap;
use std::sync::Mutex;
use ignite_project::{Project, MochiSharpTickCallback, register_script_tick_callback, unregister_script_tick_callback, trigger_script_tick};
use crate::ffi::result_ffi::IgniteResult;

static PROJECTS: Mutex<Option<HashMap<u64, Project>>> = Mutex::new(None);
static NEXT_PROJECT_ID: Mutex<u64> = Mutex::new(1);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_project_create(name_ptr: *const c_char, dir_ptr: *const c_char) -> u64 {
    if name_ptr.is_null() || dir_ptr.is_null() {
        return 0;
    }
    unsafe {
        let name_str = match CStr::from_ptr(name_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return 0,
        };
        let dir_str = match CStr::from_ptr(dir_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return 0,
        };

        let mut next_id = match NEXT_PROJECT_ID.lock() {
            Ok(g) => g,
            Err(p) => p.into_inner(),
        };
        let proj_id = *next_id;
        *next_id += 1;

        let mut lock = match PROJECTS.lock() {
            Ok(g) => g,
            Err(p) => p.into_inner(),
        };
        let map = lock.get_or_insert_with(HashMap::new);
        map.insert(proj_id, Project::new(name_str, dir_str));
        proj_id
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_project_destroy(proj_handle: u64) -> IgniteResult {
    if proj_handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    let mut lock = match PROJECTS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        if map.remove(&proj_handle).is_some() {
            return IgniteResult::Ok;
        }
    }
    IgniteResult::ErrNotFound
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_project_get_name(
    proj_handle: u64,
    out_buf: *mut c_char,
    max_len: usize,
) -> IgniteResult {
    if proj_handle == 0 || out_buf.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    let lock = match PROJECTS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(proj) = map.get(&proj_handle) {
            unsafe {
                let bytes = proj.config.name.as_bytes();
                let copy_len = bytes.len().min(max_len - 1);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
                *out_buf.add(copy_len) = 0; // null terminate
            }
            return IgniteResult::Ok;
        }
    }
    IgniteResult::ErrNotFound
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_project_get_asset_directory(
    proj_handle: u64,
    out_buf: *mut c_char,
    max_len: usize,
) -> IgniteResult {
    if proj_handle == 0 || out_buf.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    let lock = match PROJECTS.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(proj) = map.get(&proj_handle) {
            let path_str = proj.get_asset_directory_path().to_string_lossy().into_owned();
            unsafe {
                let bytes = path_str.as_bytes();
                let copy_len = bytes.len().min(max_len - 1);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
                *out_buf.add(copy_len) = 0; // null terminate
            }
            return IgniteResult::Ok;
        }
    }
    IgniteResult::ErrNotFound
}

// MochiSharp Scripting Bridge FFI
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_script_register_tick_callback(cb: MochiSharpTickCallback) -> IgniteResult {
    register_script_tick_callback(cb);
    IgniteResult::Ok
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_script_unregister_tick_callback() -> IgniteResult {
    unregister_script_tick_callback();
    IgniteResult::Ok
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_script_trigger_tick(delta_time: f32) -> bool {
    trigger_script_tick(delta_time)
}
