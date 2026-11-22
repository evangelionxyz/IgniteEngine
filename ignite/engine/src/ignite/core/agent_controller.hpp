// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_AGENT_CONTROLLER_HPP
#define IGN_AGENT_CONTROLLER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"

#include <nlohmann/json.hpp>
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace ignite
{
    class Scene;
    class Project;
    class SceneRenderer;
    class ICamera;
    class GIBaker;
    struct ApplicationCommandLineArgs;

    struct IGN_API AgentExecutionContext
    {
        Scene *scene = nullptr;
        Project *project = nullptr;
        SceneRenderer *sceneRenderer = nullptr;
        GIBaker *giBaker = nullptr;
        ICamera *camera = nullptr;

        std::function<void(const Ref<Scene> &)> onSceneReplaced;
        std::function<void()> onPlay;
        std::function<void()> onStop;
        std::function<void()> onSimulate;
    };

    class IGN_API AgentController
    {
    public:
        static AgentController &Get();

        void InitializeFromArgs(const ApplicationCommandLineArgs &args);

        void LoadCommandScript(const std::filesystem::path &scriptPath);
        void SetWatchCommandFile(const std::filesystem::path &commandFilePath, const std::filesystem::path &responseFilePath = {});
        void EnqueueCommand(const nlohmann::ordered_json &command);
        void Update(float deltaTime, AgentExecutionContext &ctx);

        static nlohmann::ordered_json ExecuteCommand(const nlohmann::ordered_json &command, AgentExecutionContext &ctx);
        static std::string ExecuteCommandJsonString(const std::string &jsonText, AgentExecutionContext &ctx);

        bool HasPendingWork() const;
        bool IsScriptCompleted() const { return m_ScriptLoaded && m_ScriptIndex >= m_ScriptCommands.size() && m_WaitFrames <= 0 && m_WaitSeconds <= 0.0f; }
        const std::vector<nlohmann::ordered_json> &GetResults() const { return m_Results; }

        static bool IsAutomationActive();

    private:
        AgentController() = default;
        void WriteReportIfConfigured();
        void WriteWatchResponse(const nlohmann::ordered_json &response);

        std::mutex m_QueueMutex;
        std::queue<nlohmann::ordered_json> m_AdhocQueue;

        std::filesystem::path m_ScriptPath;
        std::filesystem::path m_ReportPath;
        std::vector<nlohmann::ordered_json> m_ScriptCommands;
        size_t m_ScriptIndex = 0;
        bool m_ScriptLoaded = false;
        bool m_QuitAfterScript = false;
        bool m_ScriptReportWritten = false;

        std::filesystem::path m_WatchCommandPath;
        std::filesystem::path m_WatchResponsePath;
        std::string m_LastProcessedRequestId;
        std::filesystem::file_time_type m_LastWatchWriteTime{};
        bool m_HasLastWatchWriteTime = false;

        int m_WaitFrames = 0;
        float m_WaitSeconds = 0.0f;
        bool m_WasGIBaking = false;
        std::vector<nlohmann::ordered_json> m_Results;

        static std::atomic<bool> s_AutomationActive;
    };
}

#endif
