// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_SERIAL_HPP
#define IGN_CORE_SERIAL_HPP

#include "ignite/core/result.hpp"
#include "ignite/asset/asset_types.hpp"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Serialization FFI
IgniteResult ignite_serialize_metadata_yaml(uint64_t handle, const char *path, Ignite_AssetType asset_type, char *out_buf, size_t max_len);
IgniteResult ignite_deserialize_metadata_yaml(const char *yaml_str, char *out_path_buf, size_t max_len, Ignite_AssetType *out_type);
IgniteResult ignite_serialize_metadata_binary(uint64_t handle, const char *path, Ignite_AssetType asset_type, uint64_t *out_handle, const uint8_t **out_ptr, size_t *out_len);

// Asset Registry (.ixreg) File Loader FFI
size_t ignite_load_asset_registry_file(const char *file_path);

// Scene JSON Serialization FFI
uint64_t ignite_scene_load_file(const char *file_path);
uint32_t ignite_scene_get_version(uint64_t scene_handle);
size_t ignite_scene_get_entity_count(uint64_t scene_handle);
IgniteResult ignite_scene_save_file(uint64_t scene_handle, const char *file_path);
IgniteResult ignite_scene_deserialize_hierarchy_json(const char *file_path, char *out_buf, size_t max_len);
IgniteResult ignite_scene_release(uint64_t scene_handle);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_SERIAL_HPP
