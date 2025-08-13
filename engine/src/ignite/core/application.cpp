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

#include "application.hpp"

#include "ignite/graphics/shader_factory.hpp"
#include "input/app_event.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/audio/fmod_audio.hpp"
#include "ignite/physics/jolt/jolt_physics.hpp"

#include <nvrhi/utils.h>

namespace ignite
{
    static Application *s_JoltInstance = nullptr;

    Application::Application(const ApplicationCreateInfo &createInfo)
        : m_CreateInfo(createInfo)
    {
        s_JoltInstance = this;

        if (m_CreateInfo.cmdLineArgs.count > 1)
        {
            for (i32 i = 0; i < m_CreateInfo.cmdLineArgs.count; ++i)
            {
                if (strcmp(createInfo.cmdLineArgs.args[i], "-dx12") == 0)
                {
                    m_CreateInfo.graphicsApi = nvrhi::GraphicsAPI::D3D12;
                }
            }
        }

        m_CommandManager = CreateScope<CommandManager>();

        DeviceCreationParameters deviceCreateInfo;
        deviceCreateInfo.backBufferWidth = m_CreateInfo.width;
        deviceCreateInfo.backBufferHeight = m_CreateInfo.height;
        deviceCreateInfo.startMaximized = m_CreateInfo.maximized;
        deviceCreateInfo.swapChainBufferCount = 3;
        deviceCreateInfo.enablePerMonitorDPI = true;
        deviceCreateInfo.supportExplicitDisplayScaling = true;

        m_Window = CreateScope<Window>(
            m_CreateInfo.name.c_str(),
            deviceCreateInfo,
            m_CreateInfo.graphicsApi
        );

        m_Window->SetEventCallback(BIND_CLASS_EVENT_FN(Application::OnEvent));
        m_Input = Input(m_Window->GetWindowHandle());

        m_Renderer = CreateRef<Renderer>(m_Window->GetDeviceManager(), m_CreateInfo.graphicsApi);

        if (createInfo.useGui)
        {
            m_ImGuiLayer = CreateScope<ImGuiLayer>(GetDeviceManager());
            m_ImGuiLayer->OnAttach();
            // PushLayer(m_ImGuiLayer.get());
        }

        FmodAudio::Init();
        JoltPhysics::Init();
    }

    Application *Application::GetInstance()
    {
        LOG_ASSERT(s_JoltInstance, "Application has not been created!");
        return s_JoltInstance;
    }

    DeviceManager * Application::GetDeviceManager()
    {
        return GetInstance()->m_Window->GetDeviceManager();
    }

    void Application::SetWindowTitle(const std::string &title)
    {
        GetInstance()->m_Window->SetTitle(title);
    }

    void Application::UpdateAverageTimeTime(f64 elapsedTime)
    {
        m_FrameTimeSum += elapsedTime;
        m_NumberOfAccumulatedFrames++;

        if (m_FrameTimeSum >= m_AverageTimeUpdateInterval && m_NumberOfAccumulatedFrames > 0)
        {
            m_AverageFrameTime = m_FrameTimeSum / static_cast<f64>(m_NumberOfAccumulatedFrames);
            m_NumberOfAccumulatedFrames = 0;
            m_FrameTimeSum = 0.0;
        }
    }

    void Application::ProcessMainThreadSubmissions()
    {
        while (!m_ThreadFuncs.empty())
        {
            auto func = m_ThreadFuncs.front();
            if (func())
            {
                m_ThreadFuncs.pop();
            }
            else
            {
                break;
            }
        }
    }

    void Application::PushLayer(Layer *layer)
    {
        layer->OnAttach();
        m_LayerStack.PushLayer(layer);
    }

    void Application::PopLayer(Layer *layer)
    {
        layer->OnDetach();
        m_LayerStack.PopLayer(layer);
    }

    void Application::Run()
    {
        DeviceManager *deviceManager = GetDeviceManager();
        nvrhi::IDevice *device = deviceManager->GetDevice();
        auto commandList = device->createCommandList();

        while (m_Window->IsLooping())
        {
            m_Window->PollEvents();

            const f64 currTime = glfwGetTime();
            m_DeltaTime = static_cast<float>(currTime - m_PreviousTime);

            ProcessMainThreadSubmissions();

            FmodAudio::Update(m_DeltaTime);

            // update window title
            if (m_AverageFrameTime > 0)
            {
                std::stringstream ss;
                ss << m_CreateInfo.name;
                ss << " (" << nvrhi::utils::GraphicsAPIToString(device->getGraphicsAPI());
                if (deviceManager->GetDeviceParams().enableDebugRuntime)
                {
                    if (m_CreateInfo.graphicsApi == nvrhi::GraphicsAPI::VULKAN)
                        ss << ", VulkanValidationLayer";
                    else
                        ss << ", DebugRuntime";
                }

                if (deviceManager->GetDeviceParams().enableNvrhiValidationLayer)
                    ss << ", NvrhiValidationLayer";
                ss << ")";

                const f64 fps = 1.0 / m_AverageFrameTime;

                const i32 precision = (fps <= 20.0) ? 1 : 0;

                ss << " - " << std::fixed << std::setprecision(precision) << fps << " FPS ";

                m_Window->SetTitle(ss.str());
            }

            if (m_Window->IsVisible() && m_Window->IsInFocus())
            {
                // update system (physics etc..)
                for (auto layer = m_LayerStack.rbegin(); layer != m_LayerStack.rend(); ++layer)
                    (*layer)->OnUpdate(m_DeltaTime);

                // render to main framebuffer
                // begin render frame
                if (m_FrameIndex > 0)
                {
                    if (deviceManager->BeginFrame())
                    {
                        // Clearing framebuffer
                        nvrhi::IFramebuffer* framebuffer = deviceManager->GetCurrentFramebuffer();
                        commandList->open();
                        nvrhi::utils::ClearColorAttachment(commandList, framebuffer, 0, nvrhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
                        commandList->close();
                        device->executeCommandList(commandList);
                        
                        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
                        {
                            Layer *layer = *it;
                            layer->OnRender(framebuffer);

                            // ImGui rendering
                            if (m_CreateInfo.useGui)
                            {
                                m_ImGuiLayer->BeginFrame();
                                layer->OnGuiRender();
                                m_ImGuiLayer->EndFrame(framebuffer);
                            }
                        }

                        if (!deviceManager->Present())
                            continue;
                    }
                }
            }
            
            // call this at lease once per frame!
            device->runGarbageCollection();

            UpdateAverageTimeTime(m_DeltaTime);
            // set previous time
            m_PreviousTime = currTime;
            ++m_FrameIndex;
        }

        commandList = nullptr;

        device->waitForIdle();

        if (m_ImGuiLayer)
        {
            m_ImGuiLayer->OnDetach();
            m_ImGuiLayer.reset();
        }

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            (*it)->OnDetach();
            delete *it;
        }

        // destroy renderer first
        m_Renderer.reset();

        // destroy device
        deviceManager->Destroy();
        m_Window->Destroy();

        JoltPhysics::Shutdown();
        FmodAudio::Shutdown();
    }

    void Application::OnEvent(Event &e)
    {
        EventDispatcher dispatcher(e);
        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            if (e.Handled)
                break;
            (*it)->OnEvent(e);
        }
    }

    void Application::WindowIconify()
    {
        GetInstance()->m_Window->Iconify();
    }

    void Application::WindowMaximize()
    {
        GetInstance()->m_Window->Maximize();
    }

    void Application::WindowRestore()
    {
        GetInstance()->m_Window->Restore();
    }

    void Application::SubmitToMainThread(const std::function<bool()> func)
    {
        GetInstance()->m_ThreadFuncs.push(func);
    }

    CommandManager *Application::GetCommandManager()
    {
        return GetInstance()->m_CommandManager.get();
    }

    nvrhi::IDevice* Application::GetGraphicsDevice()
    {
        return GetInstance()->m_Window->GetDeviceManager()->GetDevice();
    }

    f32 Application::GetDeltaTime()
    {
        return GetInstance()->m_DeltaTime;
    }

}
