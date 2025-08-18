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
        bool maximized = false;
        bool useGui = true;
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

        static f32 GetDeltaTime();

        static void SetWindowTitle(const std::string &title);

        static void WindowIconify();
        static void WindowMaximize();
        static void WindowRestore();
        static void SubmitToMainThread(const std::function<bool()> func);

    private:
        void UpdateAverageTimeTime(f64 elapsedTime);
        void ProcessMainThreadSubmissions();

    protected:
        ApplicationCreateInfo m_CreateInfo;
        Scope<Window> m_Window;
        Scope<CommandManager> m_CommandManager;
        Scope<UIManager> m_UIManager;
        LayerStack m_LayerStack;
        Ref<ImGuiLayer> m_ImGuiLayer;
        Input m_Input;

        Ref<Renderer> m_Renderer;

        f64 m_PreviousTime = 0.0;
        f64 m_FrameTimeSum = 0.0;
        f64 m_AverageFrameTime = 0.0;
        f32 m_DeltaTime = 0.0f;
        const f64 m_AverageTimeUpdateInterval = 0.5;
        i32 m_NumberOfAccumulatedFrames = 0;
        i32 m_FrameIndex = 0;

        std::queue<std::function<bool()>> m_ThreadFuncs;
    };

    Application *CreateApplication(ApplicationCommandLineArgs args);
}
