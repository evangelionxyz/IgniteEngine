// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::collections::HashMap;
use std::sync::Mutex;
use ignite_asset::{AssetMetaData, AssetType};
use ignite_serial as serial;
use crate::ffi::result_ffi::IgniteResult;

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
