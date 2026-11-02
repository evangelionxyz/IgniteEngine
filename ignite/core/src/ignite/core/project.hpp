// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_PROJECT_HPP
#define IGN_CORE_PROJECT_HPP

#include "ignite/core/result.hpp"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MochiSharpTickCallback)(float delta_time);

// Project Management FFI
uint64_t ignite_project_create(const char *name, const char *dir_path);
IgniteResult ignite_project_destroy(uint64_t proj_handle);
IgniteResult ignite_project_get_name(uint64_t proj_handle, char *out_buf, size_t max_len);
IgniteResult ignite_project_get_asset_directory(uint64_t proj_handle, char *out_buf, size_t max_len);

// MochiSharp Scripting Bridge FFI
IgniteResult ignite_script_register_tick_callback(MochiSharpTickCallback cb);
IgniteResult ignite_script_unregister_tick_callback(void);
bool ignite_script_trigger_tick(float delta_time);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_PROJECT_HPP

