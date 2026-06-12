// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_PROFILER_HPP
#define IGN_PROFILER_HPP

#include "ignite/core/base.hpp"

#if defined(TRACY_ENABLE)
    #include <tracy/Tracy.hpp>

    #define IGN_PROFILE_FRAME() FrameMark
    #define IGN_PROFILE_FRAME_NAMED(name) FrameMarkNamed(name)
    #define IGN_PROFILE_FUNCTION() ZoneScoped
    #define IGN_PROFILE_SCOPE(name) ZoneScopedN(name)
    #define IGN_PROFILE_SCOPE_COLOR(name, color) ZoneScopedNC(name, color)
    #define IGN_PROFILE_THREAD_NAME(name) tracy::SetThreadName(name)
    #define IGN_PROFILE_PLOT(name, value) TracyPlot(name, value)
    #define IGN_PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
    #define IGN_PROFILE_FREE(ptr) TracyFree(ptr)
    #define IGN_PROFILE_ALLOC_N(ptr, size, name) TracyAllocN(ptr, size, name)
    #define IGN_PROFILE_FREE_N(ptr, name) TracyFreeN(ptr, name)
    #define IGN_PROFILE_IS_CONNECTED() tracy::GetProfiler().IsConnected()
#else
    #define IGN_PROFILE_FRAME() ((void)0)
    #define IGN_PROFILE_FRAME_NAMED(name) ((void)0)
    #define IGN_PROFILE_FUNCTION() ((void)0)
    #define IGN_PROFILE_SCOPE(name) ((void)0)
    #define IGN_PROFILE_SCOPE_COLOR(name, color) ((void)0)
    #define IGN_PROFILE_THREAD_NAME(name) ((void)0)
    #define IGN_PROFILE_PLOT(name, value) ((void)0)
    #define IGN_PROFILE_ALLOC(ptr, size) ((void)0)
    #define IGN_PROFILE_FREE(ptr) ((void)0)
    #define IGN_PROFILE_ALLOC_N(ptr, size, name) ((void)0)
    #define IGN_PROFILE_FREE_N(ptr, name) ((void)0)
    #define IGN_PROFILE_IS_CONNECTED() (false)
#endif

#endif
