// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_FRAME_H
#define IGN_RS_FRAME_H

#include "result.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Frame Lifecycle — called by C++ Application::Run() each frame
IgniteResult ignite_rs_engine_begin_frame(float delta_time);
IgniteResult ignite_rs_engine_end_frame(void);

// Frame Stats
uint64_t ignite_rs_get_frame_count(void);
double   ignite_rs_get_total_time(void);
float    ignite_rs_get_delta_time(void);
bool     ignite_rs_is_in_frame(void);
size_t   ignite_rs_get_pending_deferred_count(void);

#ifdef __cplusplus
}
#endif

#endif
