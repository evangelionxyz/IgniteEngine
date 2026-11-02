// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_RESULT_HPP
#define IGN_CORE_RESULT_HPP

#include <string>
#include <cstring>
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

const char *ignite_result_to_string(IgniteResult result);

// Returns the last error message from the Rust FFI layer.
// Thread-safe: each thread has its own error string.
// Returns the number of bytes written (excluding null terminator).
size_t ignite_get_last_error(char *out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace ignite
{
    enum class Result
    {
        Ok = 0,
        ErrNullPointer,
        ErrInvalidHandle,
        ErrInvalidParam,
        ErrNotFound,
        ErrAlreadyExists,
        ErrOperationFailed,
        ErrNotInitialized,

        ErrUnknown = 99,
    };
}

namespace ignite::rust
{
    static inline std::string GetLastError()
    {
        char buff[1024];
        std::memset(buff, 0, sizeof(buff));
        size_t size = ignite_get_last_error(buff, sizeof(buff));
        return std::string(buff, size);
    }

    static inline std::string GetResultString(Result result)
    {
        const char *str = ignite_result_to_string(static_cast<IgniteResult>(result));
        return str ? std::string(str) : "Unknown";
    }
}
#endif

#endif // IGN_CORE_RESULT_HPP
