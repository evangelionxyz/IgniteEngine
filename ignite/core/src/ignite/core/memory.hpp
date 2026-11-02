// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_MEMORY_HPP
#define IGN_CORE_MEMORY_HPP

#include "ignite/core/result.hpp"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory Allocation Verification Functions
IgniteResult ignite_test_alloc_buffer(size_t size, uint8_t fill_byte, uint64_t *out_handle, const uint8_t **out_ptr);
IgniteResult ignite_test_free_buffer(uint64_t handle);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_MEMORY_HPP

