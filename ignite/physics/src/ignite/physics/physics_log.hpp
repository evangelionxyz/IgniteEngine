// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PHYSICS_LOG_HPP
#define IGN_PHYSICS_LOG_HPP

#include "ignite/core/base.hpp"
#include <functional>
#include <string>
#include <format>
#include <cstdint>

namespace ignite::physics
{
    enum class PhysicsLogLevel : uint8_t
    {
        Trace = 0,
        Info,
        Warn,
        Error
    };

    using PhysicsLogCallback = std::function<void(PhysicsLogLevel level, const std::string &message)>;

    class IGN_API PhysicsLog
    {
    public:
        static void SetCallback(const PhysicsLogCallback &callback);
        static void Log(PhysicsLogLevel level, const std::string &message);

        template<typename... Args>
        static void Trace(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(PhysicsLogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        static void Info(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(PhysicsLogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        static void Warn(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(PhysicsLogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
        }

        template<typename... Args>
        static void Error(std::format_string<Args...> fmt, Args&&... args)
        {
            Log(PhysicsLogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
        }
    };
}

#define IGN_PHYSICS_TRACE(...) ::ignite::physics::PhysicsLog::Trace(__VA_ARGS__)
#define IGN_PHYSICS_INFO(...)  ::ignite::physics::PhysicsLog::Info(__VA_ARGS__)
#define IGN_PHYSICS_WARN(...)  ::ignite::physics::PhysicsLog::Warn(__VA_ARGS__)
#define IGN_PHYSICS_ERROR(...) ::ignite::physics::PhysicsLog::Error(__VA_ARGS__)

#endif
