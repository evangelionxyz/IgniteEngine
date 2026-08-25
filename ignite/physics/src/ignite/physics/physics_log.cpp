// Copyright (c) 2026 Evangelion Manuhutu

#include "physics_log.hpp"
#include <iostream>

namespace ignite::physics
{
    static PhysicsLogCallback s_LogCallback = nullptr;

    void PhysicsLog::SetCallback(const PhysicsLogCallback &callback)
    {
        s_LogCallback = callback;
    }

    void PhysicsLog::Log(PhysicsLogLevel level, const std::string &message)
    {
        if (s_LogCallback)
        {
            s_LogCallback(level, message);
        }
        else
        {
            switch (level)
            {
            case PhysicsLogLevel::Trace:
                std::cout << "[Physics Trace] " << message << "\n";
                break;
            case PhysicsLogLevel::Info:
                std::cout << "[Physics Info] " << message << "\n";
                break;
            case PhysicsLogLevel::Warn:
                std::cout << "[Physics Warn] " << message << "\n";
                break;
            case PhysicsLogLevel::Error:
                std::cerr << "[Physics Error] " << message << "\n";
                break;
            }
        }
    }
}
