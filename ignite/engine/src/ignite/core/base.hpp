// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_BASE_HPP
#define IGN_BASE_HPP

#include <cstdint>

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif

#ifdef _WIN32
    #ifndef PLATFORM_WINDOWS
        #define PLATFORM_WINDOWS
    #endif
    #ifdef IGN_DLL_EXPORTS
        #define IGN_API __declspec(dllexport)
    #else
        #define IGN_API __declspec(dllimport)
    #endif
#elif __linux__ || __GNUG__
    #ifndef PLATFORM_LINUX
        #define PLATFORM_LINUX
    #endif
    #ifdef IGN_DLL_EXPORTS
        #define IGN_API __attribute__((visibility("default")))
    #else
        #define IGN_API
    #endif
#endif

#if defined(_DEBUG) || defined(IGN_DEBUG_BUILD)
    #define ENABLE_ASSERTS
    #ifdef _WIN32
        #define DEBUGBREAK() __debugbreak()
    #elif __linux__
        #define DEBUGBREAK() __builtin_trap()
    #endif
#else
#define DEBUGBREAK()
#endif

#if !defined(IGN_RELEASE_BUILD) || !defined(IGN_SHIPPING_BUILD)
    #define ENABLE_VERIFY
#endif

#if defined(IGN_ENABLE_TRACY)
    #define TRACY_ENABLE
    #include <tracy/Tracy.hpp>
#endif

#if defined (IGN_TEST_BUILD)
    #define IGN_TEST_CODE
#endif

#define EXPAND_MACRO(x)
#define STRINGIFY_MACRO(x) #x

#define BIT(x)(1 << x)
#define BIND_EVENT_FN(fn) [](auto&&... args) -> decltype(auto) { return fn(std::forward<decltype(args)>(args)...); }
#define BIND_CLASS_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

// Versioning macros
// 
// [ major ][ minor ][ patch ]
// 
// 31          22 21          12 11           0
// + ------------ - +------------ - +------------ - +
// |     major    |     minor     |     patch     |
// +------------ - +------------ - +------------ - +
//       10 bits       10 bits         12 bits


#define IGN_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22U) | \
     (((uint32_t)(minor)) << 12U) | \
      ((uint32_t)(patch)))

#define IGN_VERSION_MAJOR(v) (((uint32_t)(v) >> 22U) & 0x3FFU)
#define IGN_VERSION_MINOR(v) (((uint32_t)(v) >> 12U) & 0x3FFU)
#define IGN_VERSION_PATCH(v) ((uint32_t)(v) >> & 0xFFFU)

namespace ignite::version
{
    constexpr uint32_t MakeVersion(uint32_t major, uint32_t minor, uint32_t patch)
    {
        return (major << 22U) | (minor << 12U) | patch;
    }

    constexpr uint32_t GetMajor(uint32_t ver)
    {
        return (ver >> 22U) & 0x3FFU;
    }

	constexpr uint32_t GetMinor(uint32_t ver)
	{
		return (ver >> 12U) & 0x3FFU;
	}

	constexpr uint32_t GetPatch(uint32_t ver)
	{
		return ver & 0x3FFU;
	}
}

#endif
