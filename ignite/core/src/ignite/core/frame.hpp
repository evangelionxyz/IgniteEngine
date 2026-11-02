// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_FRAME_HPP
#define IGN_CORE_FRAME_HPP

#include "ignite/core/result.hpp"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Frame Lifecycle
IgniteResult ignite_engine_begin_frame(float delta_time);
IgniteResult ignite_engine_end_frame(void);

// Frame Stats
uint64_t ignite_get_frame_count(void);
double   ignite_get_total_time(void);
float    ignite_get_delta_time(void);
bool     ignite_is_in_frame(void);
size_t   ignite_get_pending_deferred_count(void);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_FRAME_HPP

