// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "logger.hpp"
#include <memory>
#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <vector>

namespace ignite
{
    class ImGuiConsoleSink : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
            m_Messages.push_back({ msg.level, fmt::to_string(formatted) });
        }

        void flush_() override {}

        std::vector<LogMessage> m_Messages;
    };
}

struct LoggerImpl
{
    std::shared_ptr<spdlog::logger> logger;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> stdoutSink;
    std::shared_ptr<ignite::ImGuiConsoleSink> imguiSink;
};

static LoggerImpl *impl = nullptr;

namespace ignite
{
    void Logger::Init()
    {
        impl = new LoggerImpl();

        spdlog::init_thread_pool(8192, 1);
        impl->stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        impl->stdoutSink->set_pattern("%^[%T] %n: %v%$");

        impl->imguiSink = std::make_shared<ImGuiConsoleSink>();
        impl->imguiSink->set_pattern("[%T] %n: %v"); // Without color codes

        std::vector<spdlog::sink_ptr> sinks { impl->stdoutSink, impl->imguiSink };

        impl->logger = std::make_shared<spdlog::logger>(
            "[IGNITE]",
            sinks.begin(), sinks.end()
        );

        impl->logger->set_level(spdlog::level::trace);
    }

    void Logger::Shutdown()
    {
        if (impl)
        {
            impl->stdoutSink->flush();
            impl->imguiSink->flush();
            impl->logger->flush();
            delete impl;
        }
    }

    spdlog::logger *Logger::GetLogger()
    {
        return impl ? impl->logger.get() : nullptr;
    }

    const std::vector<LogMessage>& Logger::GetLogs()
    {
        return impl->imguiSink->m_Messages;
    }

    void Logger::ClearLogs()
    {
        impl->imguiSink->m_Messages.clear();
    }
}
