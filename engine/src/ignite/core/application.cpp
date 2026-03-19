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
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <nvrhi/utils.h>

namespace ignite
{
    static Application *s_AppInstance = nullptr;

    Application::Application(const ApplicationCreateInfo &createInfo)
        : m_CreateInfo(createInfo)
    {
        s_AppInstance = this;

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

        DeviceParameters deviceParams;
        deviceParams.backBufferWidth = m_CreateInfo.width;
        deviceParams.backBufferHeight = m_CreateInfo.height;
        deviceParams.startMaximized = m_CreateInfo.maximized;
		deviceParams.startBorderless = m_CreateInfo.borderless;
#if _DEBUG
        deviceParams.enableDebugRuntime = true;
#endif
        deviceParams.swapChainBufferCount = 3;
        deviceParams.enableNvrhiValidationLayer = true;
        deviceParams.enablePerMonitorDPI = true;
        deviceParams.enableGPUValidation = true;
        deviceParams.supportExplicitDisplayScaling = true;

        m_Window = CreateScope<Window>(m_CreateInfo.name.c_str(),  deviceParams, m_CreateInfo.graphicsApi );
        m_Window->SetEventCallback(BIND_CLASS_EVENT_FN(Application::OnEvent));
        m_Window->SetIcon("resources/icon.png");

        m_Input = CreateScope<Input>(m_Window.get());

        m_Renderer = CreateRef<Renderer>(m_Window->GetDeviceManager(), m_CreateInfo.graphicsApi);
        m_UIManager = CreateScope<UIManager>();

        if (createInfo.useGui)
        {
            m_ImGuiLayer = CreateScope<ImGuiLayer>(GetDeviceManager());
            m_ImGuiLayer->OnAttach();
            // PushLayer(m_ImGuiLayer.get());
        }

        if (m_CreateInfo.useAudio)
        {
            FmodAudio::Init();
        }

        if (m_CreateInfo.usePhysics)
        {
            JoltPhysics::Init();
        }
    }

    Application *Application::GetInstance()
    {
        LOG_ASSERT(s_AppInstance, "Application has not been created!");
        return s_AppInstance;
    }

    DeviceManager * Application::GetDeviceManager()
    {
        return GetInstance()->m_Window->GetDeviceManager();
    }

    void Application::SetWindowTitle(const std::string &title)
    {
        GetInstance()->m_Window->SetTitle(title);
    }

    void Application::Shutdown()
    {
        GetInstance()->m_Window->Shutdown();
    }

    void Application::UpdateAverageTimeTime(float elapsedTime)
    {
        m_FrameTimeSum += elapsedTime;
        m_NumberOfAccumulatedFrames++;

        if (m_FrameTimeSum >= m_AverageTimeUpdateInterval && m_NumberOfAccumulatedFrames > 0)
        {
            m_AverageFrameTime = m_FrameTimeSum / static_cast<float>(m_NumberOfAccumulatedFrames);
            m_NumberOfAccumulatedFrames = 0;
            m_FrameTimeSum = 0.0;
        }
    }

	void Application::ProcessRenderThreadSubmissions()
	{
		std::queue<std::function<void()>> pending;
		{
			std::lock_guard lock(m_RenderThreadFuncsMutex);
			pending.swap(m_RenderThreadFuncs);
			m_RenderThreadHasTasks = !pending.empty();
		}

		while (!pending.empty())
		{
			auto func = std::move(pending.front());
			pending.pop();
			if (func)
			{
				func();
			}
		}

		{
			std::lock_guard lock(m_RenderThreadFuncsMutex);
			m_RenderThreadHasTasks = !m_RenderThreadFuncs.empty();
		}
	}

    void Application::ProcessMainThreadSubmissions()
    {
        // Process all pending submissions
        if (!m_ThreadFuncs.empty())
        {
            std::function<void()> func;

            {
                std::lock_guard lock(m_ThreadFuncsMutex);
                func = m_ThreadFuncs.front();
            }
            
            // Execute outside lock
            if (func)
            {
                func();

                std::lock_guard lock(m_ThreadFuncsMutex);
                m_ThreadFuncs.pop();
            }
        }
    }

	void Application::RenderThreadFunc()
	{
		nvrhi::IDevice *device = GetGraphicsDevice();
		DeviceManager *deviceManager = GetDeviceManager();

		// Create per-thread command list
		auto renderCommandList = device->createCommandList();

        while (m_RenderThreadRunning)
        {
            uint64_t currentFrame;
            nvrhi::IFramebuffer *framebuffer = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_FrameMutex);
                m_FrameCV.wait(lock, [this]
                {
                    return m_CurrentFrameReady.load() || !m_RenderThreadRunning.load() || m_RenderThreadHasTasks.load();
                });

                if (!m_RenderThreadRunning) break;

                if (m_RenderThreadHasTasks.load())
                {
                    lock.unlock();
                    ProcessRenderThreadSubmissions();
                    lock.lock();

                    if (!m_CurrentFrameReady.load())
                    {
                        continue;
                    }
                }

                currentFrame = m_FrameCounter;
                m_CurrentFrameReady = false;
            }

            // Get frame resources
            uint32_t frameIndex = currentFrame % FRAMES_IN_FLIGHT;
            FrameResources &frame = m_FrameResources[frameIndex];

            // Get the current framebuffer (must be done after BeginFrame on main thread)
            framebuffer = deviceManager->GetCurrentFramebuffer();

            // Clear framebuffer
            renderCommandList->open();
            nvrhi::utils::ClearColorAttachment(renderCommandList, framebuffer, 0, nvrhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
            renderCommandList->close();
            {
                std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
                device->executeCommandList(renderCommandList);
            }

            // Render layers
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

			// Collect and execute worker command lists if any
			{
				std::lock_guard<std::mutex> lock(m_CommandListMutex);
				if (!m_PendingCommandLists.empty())
				{
					std::vector<nvrhi::ICommandList *> workerLists;
					for (auto &workerCL : m_PendingCommandLists)
						workerLists.push_back(workerCL);

					{
						std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
						device->executeCommandLists(workerLists.data(), workerLists.size());
					}
					m_PendingCommandLists.clear();
				}
			}

            // Signal frame complete
            {
                std::lock_guard<std::mutex> lock(m_FrameMutex);
                m_RenderComplete = true;
            }
            m_FrameCV.notify_all();
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
        
        // Start render thread
        m_RenderThreadRunning = true;
        m_RenderThread = CreateScope<std::thread>(&Application::RenderThreadFunc, this);
        
        SDL_Event sdlEvent;
        
        while (m_Window->IsLooping())
        {
            while (SDL_PollEvent(&sdlEvent))
            {
                m_Window->PollEvents(sdlEvent);
                if (m_CreateInfo.useGui)
                {
                    m_ImGuiLayer->PollEvent(sdlEvent);
                }
            }

            const float currTime = static_cast<float>(SDL_GetTicks());
            m_DeltaTime = static_cast<float>(currTime - m_PreviousTime) / 1000.0f;

            ProcessMainThreadSubmissions();

            if (m_CreateInfo.useAudio)
            {
                FmodAudio::Update(m_DeltaTime);
            }

            // update window title
#if 0
            if (m_AverageFrameTime > 0)
            {
                std::stringstream ss;
                ss << m_CreateInfo.name;
                ss << " (" << nvrhi::utils::GraphicsAPIToString(device->getGraphicsAPI());
                if (deviceManager->GetDeviceParameters().enableDebugRuntime)
                {
                    if (m_CreateInfo.graphicsApi == nvrhi::GraphicsAPI::VULKAN)
                        ss << ", VulkanValidationLayer";
                    else
                        ss << ", DebugRuntime";
                }

                if (deviceManager->GetDeviceParameters().enableNvrhiValidationLayer)
                {
                    ss << ", NvrhiValidationLayer";
                }
                ss << ")";

                const float fps = 1.0f / m_AverageFrameTime;

                const i32 precision = (fps <= 20.0) ? 1 : 0;

                ss << " - " << std::fixed << std::setprecision(precision) << fps << " FPS ";

                m_Window->SetTitle(ss.str());
            }
#endif

            if (m_Window->IsVisible() && m_Window->IsInFocus())
            {
                // update system (physics etc..)
                for (auto layer = m_LayerStack.rbegin(); layer != m_LayerStack.rend(); ++layer)
                    (*layer)->OnUpdate(m_DeltaTime);

                // Begin frame acquisition on main thread (required for swap chain)
                if (m_FrameIndex > 0)
                {
                    bool frameBegan = false;
                    {
                        std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
                        frameBegan = deviceManager->BeginFrame();
                    }

                    if (frameBegan)
                    {
                        // Signal render thread to start rendering
                        {
                            std::lock_guard<std::mutex> lock(m_FrameMutex);
                            m_FrameCounter++;
                            m_CurrentFrameReady = true;
                        }
                        m_FrameCV.notify_one();
                        
                        // Wait for rendering to complete
                        {
                            std::unique_lock<std::mutex> lock(m_FrameMutex);
                            m_FrameCV.wait(lock, [this] { return m_RenderComplete.load(); });
                            m_RenderComplete = false;
                        }
                        
                        // Present on main thread
                        bool presented = false;
                        {
                            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
                            presented = deviceManager->Present();
                        }

                        if (!presented)
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

        // Shutdown render thread
        m_RenderThreadRunning = false;
        m_FrameCV.notify_all();
        if (m_RenderThread && m_RenderThread->joinable())
            m_RenderThread->join();
        
		GPUUploadSync::DeviceWaitIdle(device);
        
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
        
        // destroy
        m_UIManager.reset();
        m_Renderer.reset();

        // destroy device
        deviceManager->Destroy();
        m_Window->Destroy();

        if (m_CreateInfo.usePhysics)
        {
            JoltPhysics::Shutdown();
        }

        if (m_CreateInfo.useAudio)
        {
            FmodAudio::Shutdown();
        }
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
        GetInstance()->m_Window->Minimize();
    }

    void Application::WindowMaximize()
    {
        GetInstance()->m_Window->Maximize();
    }

    void Application::WindowRestore()
    {
        GetInstance()->m_Window->Restore();
    }

    void Application::SubmitToMainThread(const std::function<void()> func)
    {
        std::lock_guard lock(GetInstance()->m_ThreadFuncsMutex);
        GetInstance()->m_ThreadFuncs.push(func);
    }

    void Application::SubmitToRenderThread(const std::function<void()> func)
    {
        if (!GetInstance()->m_RenderThreadRunning.load())
        {
            if (func)
            {
                func();
            }
            return;
        }
        {
            std::lock_guard lock(GetInstance()->m_RenderThreadFuncsMutex);
            GetInstance()->m_RenderThreadFuncs.push(func);
            GetInstance()->m_RenderThreadHasTasks = true;
        }
        GetInstance()->m_FrameCV.notify_all();
    }

    void Application::SubmitWorkerCommandList(nvrhi::CommandListHandle commandList)
    {
        std::lock_guard<std::mutex> lock(GetInstance()->m_CommandListMutex);
        GetInstance()->m_PendingCommandLists.push_back(commandList);
    }

	const std::thread *Application::GetRenderThread() const
	{
        return m_RenderThread.get();
	}

	CommandManager *Application::GetCommandManager()
    {
        return GetInstance()->m_CommandManager.get();
    }

    nvrhi::IDevice* Application::GetGraphicsDevice()
    {
        return GetInstance()->m_Window->GetDeviceManager()->GetDevice();
    }

    bool Application::IsRenderThreadRunning()
    {
        return GetInstance()->m_RenderThreadRunning.load();
    }

    float Application::GetDeltaTime()
    {
        return GetInstance()->m_DeltaTime;
    }

}
