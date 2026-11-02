// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_CORE_HPP
#define IGN_CORE_CORE_HPP

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// FFI Exported Functions
int32_t ignite_test_connection(void);

// Engine Lifecycle
bool ignite_engine_init(void);
bool ignite_engine_shutdown(void);
bool ignite_engine_is_initialized(void);

// Engine Version (packed as major << 22 | minor << 12 | patch)
uint32_t ignite_engine_get_version(void);

// UUID
uint64_t ignite_uuid_new(void);
uint64_t ignite_uuid_from_u64(uint64_t value);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_CORE_HPP

