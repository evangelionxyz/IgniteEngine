/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "layer.hpp"
#include "layer_stack.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "device/device_manager.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "input/app_event.hpp"
#include "input/input.hpp"
#include "command.hpp"
#include "ignite/graphics/ui/ui_manager.hpp"


#include <queue>
#include <mutex>
#include <filesystem>

namespace ignite
{
    class ShaderFactory;
    class Renderer;

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

    class Application
    {
    public:
        Application(const ApplicationCreateInfo &createInfo);
        virtual ~Application() = default;

        void PushLayer(Layer *layer);
        void PopLayer(Layer *layer);

        void Run();
        void OnEvent(Event &e);

        std::string GetAppName() { return m_CreateInfo.name; }
        const ApplicationCreateInfo &GetCreateInfo() { return m_CreateInfo; }

        Window *GetWindow() { return m_Window.get(); }

        static Application *GetInstance();
        static DeviceManager *GetDeviceManager();
        static CommandManager *GetCommandManager();
        static nvrhi::IDevice *GetGraphicsDevice();

        static float GetDeltaTime();

        static void SetWindowTitle(const std::string &title);
        static void Shutdown();

        static void WindowIconify();
        static void WindowMaximize();
        static void WindowRestore();
        static void SubmitToMainThread(const std::function<void()> func);
        static void SubmitToRenderThread(const std::function<void()> func);
        static void SubmitWorkerCommandList(nvrhi::CommandListHandle commandList);

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
        Scope<UIManager> m_UIManager;
        LayerStack m_LayerStack;
        Ref<ImGuiLayer> m_ImGuiLayer;
        Scope<Input> m_Input;

        Ref<Renderer> m_Renderer;

        float m_PreviousTime = 0.0f;
        float m_FrameTimeSum = 0.0f;
        float m_AverageFrameTime = 0.0f;
        float m_DeltaTime = 0.0f;
        const float m_AverageTimeUpdateInterval = 0.5f;
        int32_t m_NumberOfAccumulatedFrames = 0;
        int32_t m_FrameIndex = 0;

        std::queue<std::function<void()>> m_ThreadFuncs;
        std::mutex m_ThreadFuncsMutex;

        std::queue<std::function<void()>> m_RenderThreadFuncs;
        std::mutex m_RenderThreadFuncsMutex;
        std::atomic<bool> m_RenderThreadHasTasks{ false };

        // Rendering thread
        Scope<std::thread> m_RenderThread;
        std::atomic<bool> m_RenderThreadRunning{ false };
        std::atomic<bool> m_CurrentFrameReady{ false };
        std::atomic<bool> m_RenderComplete{ false };

        // Synchronization
        std::mutex m_CommandListMutex;
        std::vector<nvrhi::CommandListHandle> m_PendingCommandLists;

        // Frame synchronization
        std::condition_variable m_FrameCV;
        std::mutex m_FrameMutex;
        uint64_t m_FrameCounter{ 0 };

        // Per-frame resources (triple buffered)
        static constexpr uint32_t FRAMES_IN_FLIGHT = 3;
        struct FrameResources
        {
            nvrhi::CommandListHandle commandList;
            std::vector<nvrhi::CommandListHandle> workerCommandLists;
        };

        std::array<FrameResources, FRAMES_IN_FLIGHT> m_FrameResources;
    };

    Application *CreateApplication(ApplicationCommandLineArgs args);
}
