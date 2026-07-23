// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_CORE_H
#define IGN_RS_CORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// FFI Exported Functions
int32_t ignite_rust_test_connection(void);

// Engine Lifecycle
bool ignite_engine_rs_init(void);
bool ignite_engine_rs_shutdown(void);
bool ignite_engine_rs_is_initialized(void);

// UUID
uint64_t ignite_uuid_new(void);
uint64_t ignite_uuid_from_u64(uint64_t value);

#ifdef __cplusplus
}
#endif

#endif

