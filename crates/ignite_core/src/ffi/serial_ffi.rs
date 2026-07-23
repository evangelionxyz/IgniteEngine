// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::collections::HashMap;
use std::sync::Mutex;
use std::fs;
use ignite_asset::{AssetMetaData, AssetType};
use ignite_serial as serial;
use crate::ffi::result_ffi::IgniteResult;
use crate::ffi::asset_manager_ffi::ignite_rs_asset_assign_metadata;
use crate::ffi::log_ffi::{log_internal, IgniteLogLevel};

static SERIAL_BUFFERS: Mutex<Option<HashMap<u64, Vec<u8>>>> = Mutex::new(None);
static NEXT_SERIAL_HANDLE: Mutex<u64> = Mutex::new(1);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_serialize_metadata_yaml(
    handle: u64,
    path_ptr: *const c_char,
    asset_type: AssetType,
    out_buf: *mut c_char,
    max_len: usize,
) -> IgniteResult {
    if handle == 0 || path_ptr.is_null() || out_buf.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let path_str = match CStr::from_ptr(path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };
        let meta = AssetMetaData::new(path_str, asset_type);
        match serial::serialize_yaml(&meta) {
            Ok(yaml_str) => {
                let bytes = yaml_str.as_bytes();
                let copy_len = bytes.len().min(max_len - 1);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
                *out_buf.add(copy_len) = 0; // null-terminate
                IgniteResult::Ok
            }
            Err(_) => IgniteResult::ErrOperationFailed,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_deserialize_metadata_yaml(
    yaml_str_ptr: *const c_char,
    out_path_buf: *mut c_char,
    max_len: usize,
    out_type: *mut AssetType,
) -> IgniteResult {
    if yaml_str_ptr.is_null() || out_path_buf.is_null() || out_type.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let yaml_str = match CStr::from_ptr(yaml_str_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };
        match serial::deserialize_yaml::<AssetMetaData>(yaml_str) {
            Ok(meta) => {
                *out_type = meta.asset_type;
                let bytes = meta.filepath.as_bytes();
                let copy_len = bytes.len().min(max_len - 1);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_path_buf, copy_len);
                *out_path_buf.add(copy_len) = 0; // null terminate
                IgniteResult::Ok
            }
            Err(_) => IgniteResult::ErrOperationFailed,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_serialize_metadata_binary(
    handle: u64,
    path_ptr: *const c_char,
    asset_type: AssetType,
    out_handle: *mut u64,
    out_ptr: *mut *const u8,
    out_len: *mut usize,
) -> IgniteResult {
    if handle == 0 || path_ptr.is_null() || out_handle.is_null() || out_ptr.is_null() || out_len.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let path_str = match CStr::from_ptr(path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };
        let meta = AssetMetaData::new(path_str, asset_type);
        match serial::serialize_binary(&meta) {
            Ok(bytes) => {
                let len = bytes.len();
                let ptr = bytes.as_ptr();

                let mut next_id = match NEXT_SERIAL_HANDLE.lock() {
                    Ok(g) => g,
                    Err(p) => p.into_inner(),
                };
                let buf_handle = *next_id;
                *next_id += 1;

                let mut lock = match SERIAL_BUFFERS.lock() {
                    Ok(g) => g,
                    Err(p) => p.into_inner(),
                };
                let map = lock.get_or_insert_with(HashMap::new);
                map.insert(buf_handle, bytes);

                *out_handle = buf_handle;
                *out_ptr = ptr;
                *out_len = len;

                IgniteResult::Ok
            }
            Err(_) => IgniteResult::ErrOperationFailed,
        }
    }
}

/// Loads and parses an `.ixreg` YAML asset registry file in Rust,
/// registering all deserialized metadata directly into the Rust AssetManager!
#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_load_asset_registry_file(file_path_ptr: *const c_char) -> usize {
    if file_path_ptr.is_null() {
        return 0;
    }
    unsafe {
        let file_path = match CStr::from_ptr(file_path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return 0,
        };
        let yaml_str = match fs::read_to_string(file_path) {
            Ok(s) => s,
            Err(e) => {
                log_internal(IgniteLogLevel::Warn, &format!("[Rust Serializer] Failed to read asset registry file '{}': {}", file_path, e));
                return 0;
            }
        };
        match serial::AssetRegistryFile::from_yaml(&yaml_str) {
            Ok(reg_file) => {
                let mut count = 0;
                for item in reg_file.registry.assets {
                    let path_c = match std::ffi::CString::new(item.filepath) {
                        Ok(c) => c,
                        Err(_) => continue,
                    };
                    if ignite_rs_asset_assign_metadata(item.handle.into(), path_c.as_ptr(), item.asset_type) == IgniteResult::Ok {
                        count += 1;
                    }
                }
                log_internal(IgniteLogLevel::Info, &format!("[Rust Serializer] Successfully loaded {} asset entries from '{}'", count, file_path));
                count
            }
            Err(err) => {
                log_internal(IgniteLogLevel::Error, &format!("[Rust Serializer] YAML parse error in '{}': {}", file_path, err));
                0
            }
        }
    }
}
