// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_CORE_H
#define IGN_RS_CORE_H

#include "result.h"
#include "log.h"
#include "memory.h"
#include "core_utils.h"
#include "asset.h"
#include "serial.h"
#include "ecs.h"
#include "project.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// FFI Exported Functions
int32_t ignite_rs_test_connection(void);

// Engine Lifecycle
bool ignite_rs_engine_init(void);
bool ignite_rs_engine_shutdown(void);
bool ignite_rs_engine_is_initialized(void);

// UUID
uint64_t ignite_uuid_new(void);
uint64_t ignite_uuid_from_u64(uint64_t value);

#ifdef __cplusplus
}
#endif

#endif
