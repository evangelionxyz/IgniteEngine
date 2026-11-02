// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_LOG_HPP
#define IGN_CORE_LOG_HPP

#include "ignite/core/result.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum IgniteLogLevel
{
    IgniteLogLevel_Trace = 0,
    IgniteLogLevel_Info = 1,
    IgniteLogLevel_Warn = 2,
    IgniteLogLevel_Error = 3,
} IgniteLogLevel;

typedef void (*LogCallbackFn)(IgniteLogLevel level, const char *message);

IgniteResult ignite_log_register_callback(LogCallbackFn callback);
IgniteResult ignite_log_unregister_callback(void);
IgniteResult ignite_log(IgniteLogLevel level, const char *message);

#ifdef __cplusplus
}
#endif

#endif // IGN_CORE_LOG_HPP

