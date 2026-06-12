// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_APPLICATION_HPP
#define IGN_APPLICATION_HPP

#include "base.hpp"
#include "layer.hpp"
#include "layer_stack.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "device/device_manager.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/path.hpp"

#include <queue>
#include <mutex>

struct ImGuiContext;

namespace ignite
{
    class ShaderFactory;
    class Renderer;
    class Window;
    class CommandManager;
    class Input;
    class ImGuiLayer;

    struct ApplicationCommandLineArgs
    {
        i32 count = 0;
        char **args = nullptr;

        const char *operator[](int index) const
        {
            LOG_ASSERT(index < count, "Invalid index");
            return args[index];
        }
    };

    struct ApplicationCreateInfo
    {
        ApplicationCommandLineArgs cmdLineArgs;
        std::string name = "Ignite";
        std::string iconPath = " ";
        std::string workingDirectory;
        nvrhi::GraphicsAPI graphicsApi = nvrhi::GraphicsAPI::VULKAN;

        u32 width = 1280;
        u32 height = 640;
		bool borderless = false;
        bool maximized = false;
        bool useGui = true;
        bool usePhysics = true;
        bool useAudio = true;
    };

    class IGN_API Application
    {
    public:
        Application(const ApplicationCreateInfo &createInfo);
        virtual ~Application();

        void PushLayer(Layer *layer);
        void PopLayer(Layer *layer);

        void Run();
        void OnEvent(Event &e);

        std::string GetAppName() { return m_CreateInfo.name; }
        const ApplicationCreateInfo &GetCreateInfo() { return m_CreateInfo; }

        Window *GetWindow() { return m_Window.get(); }

        static Application *GetInstance();
        static CommandManager *GetCommandManager();
        static bool IsRenderThreadRunning();
        static std::thread::id GetMainThreadId();
        static ImGuiContext *GetImGuiContext();

        static float GetDeltaTime();

        static void SetWindowTitle(const std::string &title);
        static void Shutdown();

        static void WindowIconify();
        static void WindowMaximize();
        static void WindowRestore();
        static void SubmitToMainThread(const std::function<void()> func, const std::string &funcName = "MainThread");
        static void SubmitToRenderThread(const std::function<void()> func, const std::string &funcName = "RenderThread");
        static void SubmitWorkerCommandList(nvrhi::CommandListHandle commandList, std::function<void()> onExecuted = {});

        const std::thread *GetRenderThread() const;

    private:
        void UpdateAverageTimeTime(float elapsedTime);
        void ProcessMainThreadSubmissions();
        void ProcessRenderThreadSubmissions();

        void RenderThreadFunc();

    protected:
        ApplicationCreateInfo m_CreateInfo;
        Scope<Window> m_Window;
        Scope<CommandManager> m_CommandManager;
        LayerStack m_LayerStack;
        ImGuiLayer *m_ImGuiLayer;
        Scope<Input> m_Input;

        Ref<Renderer> m_Renderer;

        float m_PreviousTime = 0.0f;
        float m_FrameTimeSum = 0.0f;
        float m_AverageFrameTime = 0.0f;
        float m_DeltaTime = 0.0f;
        const float m_AverageTimeUpdateInterval = 0.5f;
        int32_t m_NumberOfAccumulatedFrames = 0;
        int32_t m_FrameIndex = 0;

        std::queue<std::pair<std::function<void()>, std::string>> m_ThreadFuncs;
        std::mutex m_ThreadFuncsMutex;

        std::queue<std::pair<std::function<void()>, std::string>> m_RenderThreadFuncs;
        std::mutex m_RenderThreadFuncsMutex;
        std::mutex m_RenderTaskMutex;
        std::condition_variable m_RenderTaskCV;
        std::atomic<bool> m_RenderThreadHasTasks{ false };

        // Rendering thread
        Scope<std::thread> m_RenderThread;
        std::thread::id m_MainThreadId;
        std::atomic<bool> m_RenderThreadRunning{ false };
        std::atomic<bool> m_CurrentFrameReady{ false };
        std::atomic<bool> m_RenderComplete{ false };

        // Synchronization
        std::mutex m_CommandListMutex;
        std::vector<nvrhi::CommandListHandle> m_PendingCommandLists;
        std::vector<std::function<void()>> m_PendingCommandListCallbacks;

        // Frame synchronization
        std::condition_variable m_FrameCV;
        std::mutex m_FrameMutex;
        uint64_t m_FrameCounter{ 0 };
    };

    Application *CreateApplication(ApplicationCommandLineArgs args);
}

#endif
