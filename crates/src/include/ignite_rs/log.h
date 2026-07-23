// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RS_LOG_H
#define IGN_RS_LOG_H

#include "result.h"

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

typedef void (*LogCallbackFn)(IgniteLogLevel level, const char* message);

IgniteResult ignite_rs_log_register_callback(LogCallbackFn callback);
IgniteResult ignite_rs_log_unregister_callback(void);
IgniteResult ignite_rs_log(IgniteLogLevel level, const char* message);

#ifdef __cplusplus
}
#endif

#endif
