// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_ECS_HPP
#define IGN_CORE_ECS_HPP

#include "ignite/core/result.hpp"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Scene Lifecycle FFI
uint64_t ignite_scene_create(const char *name);
IgniteResult ignite_scene_destroy(uint64_t scene_handle);

// Entity Operations FFI
uint64_t ignite_scene_create_entity(uint64_t scene_handle, const char *name);
IgniteResult ignite_scene_destroy_entity(uint64_t scene_handle, uint64_t entity_id);

// Entity Components FFI
IgniteResult ignite_entity_get_name(uint64_t scene_handle, uint64_t entity_id, char *out_buf, size_t max_len);
IgniteResult ignite_entity_set_transform(uint64_t scene_handle, uint64_t entity_id, const float *pos, const float *rot, const float *scale);
IgniteResult ignite_entity_get_transform(uint64_t scene_handle, uint64_t entity_id, float *out_pos, float *out_rot, float *out_scale);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_ECS_HPP

