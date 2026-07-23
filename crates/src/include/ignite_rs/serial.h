// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_SERIAL_H
#define IGN_RS_SERIAL_H

#include "result.h"
#include "asset.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Serialization FFI
IgniteResult ignite_rs_serialize_metadata_yaml(uint64_t handle, const char* path, AssetType_RS asset_type, char* out_buf, size_t max_len);
IgniteResult ignite_rs_deserialize_metadata_yaml(const char* yaml_str, char* out_path_buf, size_t max_len, AssetType_RS* out_type);
IgniteResult ignite_rs_serialize_metadata_binary(uint64_t handle, const char* path, AssetType_RS asset_type, uint64_t* out_handle, const uint8_t** out_ptr, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif
