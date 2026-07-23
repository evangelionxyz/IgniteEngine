// Copyright (c) 2026 Evangelion Manuhutu

use std::os::raw::c_char;
use std::ffi::CStr;
use std::collections::HashMap;
use std::sync::Mutex;
use ignite_ecs::{Scene, EntityId, TransformComponent};
use crate::ffi::result_ffi::IgniteResult;

static SCENES: Mutex<Option<HashMap<u64, Scene>>> = Mutex::new(None);
static NEXT_SCENE_ID: Mutex<u64> = Mutex::new(1);

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_scene_create(name_ptr: *const c_char) -> u64 {
    let name_str = if !name_ptr.is_null() {
        unsafe { CStr::from_ptr(name_ptr).to_str().unwrap_or("Untitled Scene") }
    } else {
        "Untitled Scene"
    };

    let mut next_id = match NEXT_SCENE_ID.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    let scene_id = *next_id;
    *next_id += 1;

    let mut lock = match SCENES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    let map = lock.get_or_insert_with(HashMap::new);
    map.insert(scene_id, Scene::new(name_str));
    scene_id
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_scene_destroy(scene_handle: u64) -> IgniteResult {
    if scene_handle == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    let mut lock = match SCENES.lock() {
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

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_scene_create_entity(scene_handle: u64, name_ptr: *const c_char) -> u64 {
    if scene_handle == 0 {
        return 0;
    }
    let name_str = if !name_ptr.is_null() {
        unsafe { CStr::from_ptr(name_ptr).to_str().unwrap_or("Entity") }
    } else {
        "Entity"
    };

    let mut lock = match SCENES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        if let Some(scene) = map.get_mut(&scene_handle) {
            let entity_id = scene.create_entity(name_str);
            return entity_id.0;
        }
    }
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_scene_destroy_entity(scene_handle: u64, entity_id: u64) -> IgniteResult {
    if scene_handle == 0 || entity_id == 0 {
        return IgniteResult::ErrInvalidHandle;
    }
    let mut lock = match SCENES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        if let Some(scene) = map.get_mut(&scene_handle) {
            if scene.destroy_entity(EntityId(entity_id)) {
                return IgniteResult::Ok;
            }
        }
    }
    IgniteResult::ErrNotFound
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_entity_get_name(
    scene_handle: u64,
    entity_id: u64,
    out_buf: *mut c_char,
    max_len: usize,
) -> IgniteResult {
    if scene_handle == 0 || entity_id == 0 || out_buf.is_null() || max_len == 0 {
        return IgniteResult::ErrNullPointer;
    }
    let lock = match SCENES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(scene) = map.get(&scene_handle) {
            if let Some(tag) = scene.get_tag(EntityId(entity_id)) {
                unsafe {
                    let bytes = tag.name.as_bytes();
                    let copy_len = bytes.len().min(max_len - 1);
                    std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out_buf, copy_len);
                    *out_buf.add(copy_len) = 0; // null-terminate
                }
                return IgniteResult::Ok;
            }
        }
    }
    IgniteResult::ErrNotFound
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_entity_set_transform(
    scene_handle: u64,
    entity_id: u64,
    pos: *const f32,
    rot: *const f32,
    scale: *const f32,
) -> IgniteResult {
    if scene_handle == 0 || entity_id == 0 || pos.is_null() || rot.is_null() || scale.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    let mut lock = match SCENES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_mut() {
        if let Some(scene) = map.get_mut(&scene_handle) {
            unsafe {
                let transform = TransformComponent {
                    translation: [*pos, *pos.add(1), *pos.add(2)],
                    rotation: [*rot, *rot.add(1), *rot.add(2)],
                    scale: [*scale, *scale.add(1), *scale.add(2)],
                };
                if scene.set_transform(EntityId(entity_id), transform) {
                    return IgniteResult::Ok;
                }
            }
        }
    }
    IgniteResult::ErrNotFound
}

#[unsafe(no_mangle)]
pub extern "C" fn ignite_rs_entity_get_transform(
    scene_handle: u64,
    entity_id: u64,
    out_pos: *mut f32,
    out_rot: *mut f32,
    out_scale: *mut f32,
) -> IgniteResult {
    if scene_handle == 0 || entity_id == 0 || out_pos.is_null() || out_rot.is_null() || out_scale.is_null() {
        return IgniteResult::ErrNullPointer;
    }
    let lock = match SCENES.lock() {
        Ok(g) => g,
        Err(p) => p.into_inner(),
    };
    if let Some(map) = lock.as_ref() {
        if let Some(scene) = map.get(&scene_handle) {
            if let Some(transform) = scene.get_transform(EntityId(entity_id)) {
                unsafe {
                    *out_pos = transform.translation[0];
                    *out_pos.add(1) = transform.translation[1];
                    *out_pos.add(2) = transform.translation[2];

                    *out_rot = transform.rotation[0];
                    *out_rot.add(1) = transform.rotation[1];
                    *out_rot.add(2) = transform.rotation[2];

                    *out_scale = transform.scale[0];
                    *out_scale.add(1) = transform.scale[1];
                    *out_scale.add(2) = transform.scale[2];
                }
                return IgniteResult::Ok;
            }
        }
    }
    IgniteResult::ErrNotFound
}
