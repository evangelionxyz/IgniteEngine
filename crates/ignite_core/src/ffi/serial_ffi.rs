// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::collections::HashMap;
use std::sync::Mutex;
use std::fs;
use ignite_asset::{AssetMetaData, AssetType};
use ignite_serial as serial;
use crate::ffi::result_ffi::IgniteResult;
use crate::ffi::asset_manager_ffi::ignite_asset_assign_metadata;
use crate::ffi::log_ffi::{log_internal, IgniteLogLevel};

static SERIAL_BUFFERS: Mutex<Option<HashMap<u64, Vec<u8>>>> = Mutex::new(None);
static NEXT_SERIAL_HANDLE: Mutex<u64> = Mutex::new(1);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_serialize_metadata_yaml(
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
pub extern "C" fn ignite_deserialize_metadata_yaml(
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
pub extern "C" fn ignite_serialize_metadata_binary(
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
pub extern "C" fn ignite_load_asset_registry_file(file_path_ptr: *const c_char) -> usize {
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
                    if ignite_asset_assign_metadata(item.handle.into(), path_c.as_ptr(), item.asset_type) == IgniteResult::Ok {
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

// -------------------------------------------------
// Scene JSON Serialization FFI
// -------------------------------------------------

static SCENE_FILES: Mutex<Option<HashMap<u64, serial::scene::SceneFile>>> = Mutex::new(None);
static NEXT_SCENE_FILE_HANDLE: Mutex<u64> = Mutex::new(1);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_scene_load_file(file_path_ptr: *const c_char) -> u64 {
    if file_path_ptr.is_null() {
        return 0;
    }
    unsafe {
        let file_path = match CStr::from_ptr(file_path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return 0,
        };

        match serial::scene::SceneFile::from_file(file_path) {
            Ok(scene_file) => {
                let mut next_id = match NEXT_SCENE_FILE_HANDLE.lock() {
                    Ok(g) => g,
                    Err(p) => p.into_inner(),
                };
                let handle = *next_id;
                *next_id += 1;

                let entity_count = scene_file.scene.entities.len();
                let version = scene_file.scene.version;

                let mut lock = match SCENE_FILES.lock() {
                    Ok(g) => g,
                    Err(p) => p.into_inner(),
                };
                let map = lock.get_or_insert_with(HashMap::new);
                map.insert(handle, scene_file);

                log_internal(
                    IgniteLogLevel::Info,
                    &format!(
                        "[Rust Scene Serializer] Loaded scene '{}' (v{}, {} entities, handle: {})",
                        file_path, version, entity_count, handle
                    ),
                );

                handle
            }
            Err(e) => {
                log_internal(
                    IgniteLogLevel::Error,
                    &format!("[Rust Scene Serializer] Failed to load scene from '{}': {}", file_path, e),
                );
                0
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_scene_get_version(scene_handle: u64) -> u32 {
    if scene_handle == 0 {
        return 0;
    }
    let lock = match SCENE_FILES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(scene_file) = map.get(&scene_handle) {
            return scene_file.scene.version;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_scene_get_entity_count(scene_handle: u64) -> usize {
    if scene_handle == 0 {
        return 0;
    }
    let lock = match SCENE_FILES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(scene_file) = map.get(&scene_handle) {
            return scene_file.scene.entities.len();
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_scene_save_file(scene_handle: u64, file_path_ptr: *const c_char) -> IgniteResult {
    if scene_handle == 0 || file_path_ptr.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let file_path = match CStr::from_ptr(file_path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };

        let lock = match SCENE_FILES.lock() {
            Ok(g) => g,
            Err(p) => p.into_inner(),
        };
        if let Some(map) = lock.as_ref() {
            if let Some(scene_file) = map.get(&scene_handle) {
                return match scene_file.to_file(file_path) {
                    Ok(_) => {
                        log_internal(
                            IgniteLogLevel::Info,
                            &format!("[Rust Scene Serializer] Saved scene to '{}'", file_path),
                        );
                        IgniteResult::Ok
                    }
                    Err(e) => {
                        log_internal(
                            IgniteLogLevel::Error,
                            &format!("[Rust Scene Serializer] Failed to write scene to '{}': {}", file_path, e),
                        );
                        IgniteResult::ErrOperationFailed
                    }
                };
            }
        }
        IgniteResult::ErrNotFound
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_scene_deserialize_hierarchy_json(
    file_path_ptr: *const c_char,
    out_buf: *mut c_char,
    max_len: usize,
) -> IgniteResult {
    if file_path_ptr.is_null() || out_buf.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    unsafe {
        let file_path = match CStr::from_ptr(file_path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };

        match serial::scene::SceneFile::from_file(file_path) {
            Ok(scene_file) => match scene_file.to_hierarchy_json() {
                Ok(json_str) => {
                    let bytes = json_str.as_bytes();
                    let copy_len = bytes.len().min(max_len - 1);
                    std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
                    *out_buf.add(copy_len) = 0;
                    IgniteResult::Ok
                }
                Err(_) => IgniteResult::ErrOperationFailed,
            },
            Err(_) => IgniteResult::ErrNotFound,
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_scene_release(scene_handle: u64) -> IgniteResult {
    if scene_handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    let mut lock = match SCENE_FILES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        if map.remove(&scene_handle).is_some() {
            return IgniteResult::Ok;
        }
    }
    IgniteResult::ErrNotFound
}


// -------------------------------------------------
// Project (.ixproj) Serialization FFI
// -------------------------------------------------

static PROJECT_FILES: Mutex<Option<HashMap<u64, serial::ProjectFile>>> = Mutex::new(None);
static NEXT_PROJECT_FILE_HANDLE: Mutex<u64> = Mutex::new(1);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_project_file_load(file_path_ptr: *const c_char) -> u64 {
    if file_path_ptr.is_null() {
        return 0;
    }
    unsafe {
        let file_path = match CStr::from_ptr(file_path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return 0,
        };

        match serial::ProjectFile::from_file(file_path) {
            Ok(proj_file) => {
                let mut next_id = match NEXT_PROJECT_FILE_HANDLE.lock() {
                    Ok(g) => g,
                    Err(p) => p.into_inner(),
                };
                let handle = *next_id;
                *next_id += 1;

                let name = proj_file.project.name.clone();

                let mut lock = match PROJECT_FILES.lock() {
                    Ok(g) => g,
                    Err(p) => p.into_inner(),
                };
                let map = lock.get_or_insert_with(HashMap::new);
                map.insert(handle, proj_file);

                log_internal(
                    IgniteLogLevel::Info,
                    &format!("[Rust Project Serializer] Loaded project '{}' from '{}' (handle: {})", name, file_path, handle),
                );

                handle
            }
            Err(e) => {
                log_internal(
                    IgniteLogLevel::Error,
                    &format!("[Rust Project Serializer] Failed to load project from '{}': {}", file_path, e),
                );
                0
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_project_file_save(proj_handle: u64, out_path_ptr: *const c_char) -> IgniteResult {
    if proj_handle == 0 || out_path_ptr.is_null() {
        return IgniteResult::ErrInvalidHandle;
    }
    unsafe {
        let out_path = match CStr::from_ptr(out_path_ptr).to_str() {
            Ok(s) => s,
            Err(_) => return IgniteResult::ErrInvalidParam,
        };

        let lock = match PROJECT_FILES.lock() {
            Ok(g) => g,
            Err(p) => p.into_inner(),
        };
        if let Some(map) = lock.as_ref() {
            if let Some(proj_file) = map.get(&proj_handle) {
                return match proj_file.save_file(out_path) {
                    Ok(_) => IgniteResult::Ok,
                    Err(_) => IgniteResult::ErrOperationFailed,
                };
            }
        }
        IgniteResult::ErrNotFound
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_project_file_get_name(
    proj_handle: u64,
    out_buf: *mut c_char,
    max_len: usize,
) -> IgniteResult {
    if proj_handle == 0 || out_buf.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    let lock = match PROJECT_FILES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(proj_file) = map.get(&proj_handle) {
            unsafe {
                let bytes = proj_file.project.name.as_bytes();
                let copy_len = bytes.len().min(max_len - 1);
                std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
                *out_buf.add(copy_len) = 0;
            }
            return IgniteResult::Ok;
        }
    }
    IgniteResult::ErrNotFound
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_project_file_get_server_port(proj_handle: u64) -> u16 {
    if proj_handle == 0 {
        return 0;
    }
    let lock = match PROJECT_FILES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(proj_file) = map.get(&proj_handle) {
            if let Some(ref server) = proj_file.project.server {
                return server.port;
            }
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_project_file_get_server_tick_rate(proj_handle: u64) -> u16 {
    if proj_handle == 0 {
        return 0;
    }
    let lock = match PROJECT_FILES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(proj_file) = map.get(&proj_handle) {
            if let Some(ref server) = proj_file.project.server {
                return server.tick_rate;
            }
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_project_file_release(proj_handle: u64) -> IgniteResult {
    if proj_handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    let mut lock = match PROJECT_FILES.lock() {
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
