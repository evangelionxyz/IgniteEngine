// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_CORE_UTILS_H
#define IGN_RS_CORE_UTILS_H

#include "result.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// String Utilities
bool ignite_rs_string_ends_with(const char* str, const char* ending);
size_t ignite_rs_string_to_lower(const char* str, char* out_buf, size_t max_len);
size_t ignite_rs_string_trim(const char* str, char* out_buf, size_t max_len);

// Hashing
uint64_t ignite_rs_hash_combine(uint64_t seed, uint64_t hash_val);
uint64_t ignite_rs_hash_string(const char* str);

// Timers
uint64_t ignite_rs_timer_create(void);
bool ignite_rs_timer_reset(uint64_t timer_id);
float ignite_rs_timer_elapsed_seconds(uint64_t timer_id);
bool ignite_rs_timer_destroy(uint64_t timer_id);

// SignalBus
IgniteResult ignite_rs_signal_publish(const char* signal_name, const uint8_t* payload_ptr, size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif
