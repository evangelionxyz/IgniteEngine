// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#ifndef IGN_TIME_HPP
#define IGN_TIME_HPP

#include "types.hpp"
#include <chrono>
#include <ctime>

namespace ignite
{
    class Timestep
    {
    public:
        explicit Timestep(const float time = 0.0)
            : m_Time(time)
        {
        }

        explicit operator float() const { return m_Time; }
        [[nodiscard]] float Seconds() const { return m_Time; }
        [[nodiscard]] float MilliSeconds() const { return m_Time * 1000.0f; }

        [[nodiscard]] static constexpr std::tm GetLocalTime()
        {
			const auto now = std::chrono::system_clock::now();
			const auto time_t_now = std::chrono::system_clock::to_time_t(now);
            return *std::localtime(&time_t_now);
        }

    private:
        float m_Time;
    };

    class Timer
    {
    public:
        Timer()
        {
            Reset();
        }

        void Reset()
        {
            m_Start = std::chrono::high_resolution_clock::now();
        }

        [[nodiscard]] float Elapsed() const
        {
            return static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_Start).count()) * 0.001f * 0.001f * 0.001f;
        }

        [[nodiscard]] float ElapsedMillis() const
        {
            return Elapsed() * 1000.0f;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
    };
}

#endif
