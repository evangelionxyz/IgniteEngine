// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_RESULT_H
#define IGN_RS_RESULT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum IgniteResult
{
    IgniteResult_Ok = 0,
    IgniteResult_ErrNullPointer = 1,
    IgniteResult_ErrInvalidHandle = 2,
    IgniteResult_ErrInvalidParam = 3,
    IgniteResult_ErrNotFound = 4,
    IgniteResult_ErrAlreadyExists = 5,
    IgniteResult_ErrOperationFailed = 6,
    IgniteResult_ErrNotInitialized = 7,
    IgniteResult_ErrUnknown = 99,
} IgniteResult;

const char* ignite_result_to_string(IgniteResult result);

// Returns the last error message from the Rust FFI layer.
// Thread-safe: each thread has its own error string.
// Returns the number of bytes written (excluding null terminator).
size_t ignite_rs_get_last_error(char* out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif

