// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "application.hpp"
#include "input/app_event.hpp"
#include "ignite/imgui/imgui_layer.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/audio/fmod_audio.hpp"
#include "ignite/physics/jolt/jolt_physics.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/window.hpp"
#include "input/input.hpp"
#include "command.hpp"
#include <nvrhi/utils.h>

namespace ignite
{
    static Application *s_AppInstance = nullptr;

    Application::~Application() = default;

    Application::Application(const ApplicationCreateInfo &createInfo)
        : m_CreateInfo(createInfo)
    {
        IGN_PROFILE_FUNCTION();
        s_AppInstance = this;
        m_MainThreadId = std::this_thread::get_id();

        // Input user arguments
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
        deviceParams.swapChainBufferCount = 3; // TRIPLE BUFFER
        deviceParams.enableNvrhiValidationLayer = true;
        deviceParams.enablePerMonitorDPI = true;
        deviceParams.enableGPUValidation = true;
        deviceParams.supportExplicitDisplayScaling = true;

        m_Window = CreateScope<Window>(m_CreateInfo.name.c_str(),  deviceParams, m_CreateInfo.graphicsApi );
        m_Window->SetEventCallback(BIND_CLASS_EVENT_FN(Application::OnEvent));
        m_Window->SetIcon("resources/ignite-icon256px.png");

        m_Input = CreateScope<Input>(m_Window.get());

        m_Renderer = CreateRef<Renderer>(m_Window->GetDeviceManager(), m_CreateInfo.graphicsApi);

        if (createInfo.useGui)
        {
            m_ImGuiLayer = new ImGuiLayer(m_Window->GetDeviceManager());
            PushLayer(m_ImGuiLayer);
        }

        AssetWorker::Init();

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
        IGN_PROFILE_FUNCTION();
        std::queue<std::pair<std::function<void()>, std::string>> pending;

        {
            std::lock_guard lock(m_RenderThreadFuncsMutex);
            pending.swap(m_RenderThreadFuncs);
            m_RenderThreadHasTasks = !pending.empty();
        }

        while (!pending.empty())
        {
            auto func = std::move(pending.front());
            pending.pop();
            if (func.first)
            {
                func.first();
            }
        }

        {
            std::lock_guard lock(m_RenderThreadFuncsMutex);
            m_RenderThreadHasTasks = !m_RenderThreadFuncs.empty();
        }
    }

    void Application::ProcessMainThreadSubmissions()
    {
        IGN_PROFILE_FUNCTION();

        std::queue<std::pair<std::function<void()>, std::string>> pending;
        
        {
            std::lock_guard lock(m_ThreadFuncsMutex);
            pending.swap(m_ThreadFuncs);
        }

        while (!pending.empty())
        {
            auto func = std::move(pending.front());
            pending.pop();
            if (func.first)
            {
                func.first();
            }
        }
    }

    // ------------------------------
    // Running/process render thread
    // ------------------------------
    void Application::RenderThreadFunc()
    {
        IGN_PROFILE_THREAD_NAME("Render Thread");
        IGN_PROFILE_SCOPE("Application::RenderThread");
        DeviceManager *deviceManager = m_Window->GetDeviceManager();
        nvrhi::IDevice *device = deviceManager->GetDevice();

        // Create per-thread command list
        auto renderCommandList = device->createCommandList();

        while (m_RenderThreadRunning)
        {
            // ---------------------------------------------------------------
            // Frame takes absolute priority over task processing.
            // If the main thread has already signalled a frame-ready, jump
            // straight to executing it so we never stall m_RenderComplete.
            // Only drain pre-frame render-thread tasks when no frame is pending.
            // ---------------------------------------------------------------
            if (!m_CurrentFrameReady.load() && m_RenderThreadHasTasks.load())
            {
                IGN_PROFILE_SCOPE("RenderThread::PreFrameSubmissions");
                ProcessRenderThreadSubmissions();
            }

            if (!m_RenderThreadRunning)
                break;

            uint64_t currentFrame = 0;
            {
                IGN_PROFILE_SCOPE("RenderThread::WaitForFrameReady");
                std::unique_lock<std::mutex> lock(m_FrameMutex);

                // Wait up to 2 ms for a frame OR until a task arrives so we
                // loop back and check m_RenderThreadHasTasks quickly.
                m_FrameCV.wait_for(lock, std::chrono::milliseconds(2), [this] { return m_CurrentFrameReady.load() || !m_RenderThreadRunning.load(); });

                if (!m_RenderThreadRunning)
                    break;

                if (!m_CurrentFrameReady.load())
                {
                    // No frame yet — wake the render-task CV briefly so we
                    // re-check tasks on the next iteration without extra latency.
                    m_RenderTaskCV.notify_one();
                    continue;
                }

                currentFrame = m_FrameCounter;
                m_CurrentFrameReady = false;
            }

            IGN_PROFILE_SCOPE("RenderThread::Frame");

            // Get the current framebuffer (must be done after BeginFrame on main thread)
            nvrhi::IFramebuffer *backBufferFrameBuffer = deviceManager->GetCurrentFramebuffer();

            // ------------------------------
            // Clear Back Buffer Framebuffer
            // ------------------------------
            {
                IGN_PROFILE_SCOPE("RenderThread::ClearFramebuffer");
                renderCommandList->open();
                nvrhi::utils::ClearColorAttachment(renderCommandList, backBufferFrameBuffer, 0, nvrhi::Color(0.0f, 0.0f, 0.0f, 1.0f));
                renderCommandList->close();
                {
                    auto &queueMutex = GPUUploadSync::GetQueueMutex();
                    std::unique_lock<std::mutex> queueLock(queueMutex, std::defer_lock);
                    {
                        IGN_PROFILE_SCOPE("RenderThread::ClearFramebuffer::QueueMutexWait");
                        queueLock.lock();
                    }
                    {
                        IGN_PROFILE_SCOPE("RenderThread::ClearFramebuffer::QueueMutexHold");
                        device->executeCommandList(renderCommandList);
                    }
                }
            }

            // Record statistics
            Renderer::BeginStats();

            // Render layers
            {
                IGN_PROFILE_SCOPE("RenderThread::LayerRender");
                for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
                {
                    Layer *layer = *it;
                    layer->OnRender(backBufferFrameBuffer);
                }
            }

            if (m_CreateInfo.useGui && m_ImGuiLayer)
            {
                IGN_PROFILE_SCOPE("RenderThread::ImGuiBeginFrame");
                m_ImGuiLayer->BeginFrame();
            }

            if (m_CreateInfo.useGui)
            {
                IGN_PROFILE_SCOPE("RenderThread::OnGuiRender");
                for (auto it = m_LayerStack.begin(); it != m_LayerStack.end(); ++it)
                {
                    Layer *layer = *it;
                    if (layer == m_ImGuiLayer)
                        continue;

                    layer->OnGuiRender();
                }
            }

            if (m_CreateInfo.useGui && m_ImGuiLayer)
            {
                IGN_PROFILE_SCOPE("RenderThread::ImGuiEndFrame");
                m_ImGuiLayer->EndFrame(backBufferFrameBuffer);

                IGN_PROFILE_SCOPE("RenderThread::ImGuiRenderPlatformWindows");
                m_ImGuiLayer->RenderPlatformWindows();
            }

            // Collect worker command lists with minimal lock hold.
            std::vector<nvrhi::CommandListHandle> pendingWorkerCommandLists;
            std::vector<std::function<void()>> callbacks;
            {
                std::lock_guard<std::mutex> lock(m_CommandListMutex);
                if (!m_PendingCommandLists.empty())
                {
                    pendingWorkerCommandLists.swap(m_PendingCommandLists);
                    callbacks.swap(m_PendingCommandListCallbacks);
                }
            }

            if (!pendingWorkerCommandLists.empty())
            {
                IGN_PROFILE_SCOPE("RenderThread::WorkerSubmit");
                std::vector<nvrhi::ICommandList *> workerLists;
                workerLists.reserve(pendingWorkerCommandLists.size());
                for (auto &workerCL : pendingWorkerCommandLists)
                {
                    workerLists.push_back(workerCL);
                }

                {
                    auto &queueMutex = GPUUploadSync::GetQueueMutex();
                    std::unique_lock<std::mutex> queueLock(queueMutex, std::defer_lock);
                    {
                        IGN_PROFILE_SCOPE("RenderThread::WorkerSubmit::QueueMutexWait");
                        queueLock.lock();
                    }
                    {
                        IGN_PROFILE_SCOPE("RenderThread::WorkerSubmit::QueueMutexHold");
                        device->executeCommandLists(workerLists.data(), workerLists.size());
                    }
                }
            }

            // Signal frame complete as soon as all GPU work for this frame has been submitted.
            {
                std::lock_guard<std::mutex> lock(m_FrameMutex);
                m_RenderComplete = true;
            }
            m_FrameCV.notify_all();

            for (auto &callback : callbacks)
            {
                if (callback)
                {
                    callback();
                }
            }

            // Drain any render-thread tasks that arrived during the frame.
            if (m_RenderThreadHasTasks.load())
            {
                IGN_PROFILE_SCOPE("RenderThread::PostFrameSubmissions");
                ProcessRenderThreadSubmissions();
            }
        }
    }

    void Application::PushLayer(Layer *layer)
    {
        IGN_PROFILE_FUNCTION();
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PopLayer(Layer *layer)
    {
        IGN_PROFILE_FUNCTION();
        layer->OnDetach();
        m_LayerStack.PopLayer(layer);
    }

    void Application::Run()
    {
        IGN_PROFILE_THREAD_NAME("Main Thread");
        IGN_PROFILE_SCOPE("Application::Run");
        DeviceManager *deviceManager = m_Window->GetDeviceManager();
        nvrhi::IDevice *device = deviceManager->GetDevice();
        
        // Start render thread
        m_RenderThreadRunning = true;
        m_RenderThread = CreateScope<std::thread>(&Application::RenderThreadFunc, this);

        std::stringstream ss;
        ss << m_RenderThread->get_id();
        unsigned long long id = std::stoull(ss.str());
        LOG_WARN("[Application] Render thread: {}", id);
        
        SDL_Event sdlEvent;
        
        while (m_Window->IsLooping())
        {
            IGN_PROFILE_SCOPE("MainThread::Frame");
            while (SDL_PollEvent(&sdlEvent))
            {
                m_Window->PollEvents(sdlEvent);
                if (m_CreateInfo.useGui)
                {
                    m_ImGuiLayer->PollEvent(sdlEvent);
                }

                for (auto layer = m_LayerStack.rbegin(); layer != m_LayerStack.rend(); ++layer)
                {
                    (*layer)->OnSDLEvent(&sdlEvent);
                }
            }

            const float currTime = static_cast<float>(SDL_GetTicks());
            m_DeltaTime = static_cast<float>(currTime - m_PreviousTime) / 1000.0f;
            IGN_PROFILE_PLOT("Delta Time (s)", m_DeltaTime);

            ProcessMainThreadSubmissions();

            if (m_CreateInfo.useAudio)
            {
                FmodAudio::Update(m_DeltaTime);
            }

            if (m_Window->IsVisible() && m_Window->IsInFocus())
            {
                IGN_PROFILE_SCOPE("MainThread::SimulationAndPresent");

                for (auto layer = m_LayerStack.rbegin(); layer != m_LayerStack.rend(); ++layer)
                    (*layer)->OnUpdate(m_DeltaTime);

                if (m_FrameIndex > 0)
                {
                    bool frameBegan = false;
                    {
                        IGN_PROFILE_SCOPE("MainThread::BeginFrame");
                        auto &queueMutex = GPUUploadSync::GetQueueMutex();
                        std::unique_lock<std::mutex> queueLock(queueMutex, std::defer_lock);
                        {
                            IGN_PROFILE_SCOPE("MainThread::BeginFrame::QueueMutexWait");
                            queueLock.lock();
                        }
                        {
                            IGN_PROFILE_SCOPE("MainThread::BeginFrame::QueueMutexHold");
                            frameBegan = deviceManager->BeginFrame();
                        }
                    }

                    if (frameBegan)
                    {
                        {
                            std::lock_guard<std::mutex> lock(m_FrameMutex);
                            m_FrameCounter++;
                            m_CurrentFrameReady = true;
                        }
                        m_FrameCV.notify_one();
                        
                        {
                            IGN_PROFILE_SCOPE("MainThread::WaitForRenderComplete");
                            std::unique_lock<std::mutex> lock(m_FrameMutex);

                            while (!m_RenderComplete.load())
                            {
                                const bool signaled = m_FrameCV.wait_for(lock, std::chrono::microseconds(500), [this] 
                                { 
                                    return m_RenderComplete.load();
                                });

                                if (signaled)
                                    break;
                            }

                            m_RenderComplete = false;
                        }

                        if (m_CreateInfo.useGui && m_ImGuiLayer)
                        {
                            IGN_PROFILE_SCOPE("MainThread::ImGuiRenderPlatformWindows");
                            m_ImGuiLayer->RenderPlatformWindows();
                        }
                        
                        bool presented = false;
                        {
                            IGN_PROFILE_SCOPE("MainThread::Present");
                            std::unique_lock<std::mutex> queueLock(GPUUploadSync::GetQueueMutex(), std::defer_lock);
                            {
                                IGN_PROFILE_SCOPE("MainThread::Present::QueueMutexWait");
                                queueLock.lock();
                            }
                            {
                                IGN_PROFILE_SCOPE("MainThread::Present::QueueMutexHold");
                                presented = deviceManager->Present();
                            }
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
            IGN_PROFILE_FRAME_NAMED("Main Frame");
        }

        // Shutdown render thread
        m_RenderThreadRunning = false;
        m_FrameCV.notify_all();
        m_RenderTaskCV.notify_all();
        if (m_RenderThread && m_RenderThread->joinable())
            m_RenderThread->join();

        AssetWorker::Shutdown();
        
        GPUUploadSync::DeviceWaitIdle(device);
        
        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            (*it)->OnDetach();
            delete *it;
        }
        
        // destroy
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

        if (m_CreateInfo.useGui && m_ImGuiLayer)
        {
            m_ImGuiLayer->OnEvent(e);
        }

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            if (e.Handled)
                break;

            if (*it == m_ImGuiLayer)
                continue;

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

    void Application::SubmitToMainThread(const std::function<void()> func, const std::string &funcName)
    {
        std::lock_guard lock(GetInstance()->m_ThreadFuncsMutex);
        GetInstance()->m_ThreadFuncs.push({ func, funcName });
    }

    void Application::SubmitToRenderThread(const std::function<void()> func, const std::string &funcName)
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
            GetInstance()->m_RenderThreadFuncs.push({ func, funcName });
            GetInstance()->m_RenderThreadHasTasks = true;
        }
        // Wake the render thread:
        // - m_RenderTaskCV: dedicated task signal (used by the pre-frame check)
        // - m_FrameCV: wakes the render thread out of its frame wait_for so it
        //   can loop back immediately and process the new task without waiting
        //   the full 2ms timeout.
        GetInstance()->m_RenderTaskCV.notify_one();
        GetInstance()->m_FrameCV.notify_one();
    }

    void Application::SubmitWorkerCommandList(nvrhi::CommandListHandle commandList, std::function<void()> onExecuted)
    {
        std::lock_guard<std::mutex> lock(GetInstance()->m_CommandListMutex);
        GetInstance()->m_PendingCommandLists.push_back(commandList);
        GetInstance()->m_PendingCommandListCallbacks.push_back(std::move(onExecuted));
    }

    const std::thread *Application::GetRenderThread() const
    {
        return m_RenderThread.get();
    }

    CommandManager *Application::GetCommandManager()
    {
        return GetInstance()->m_CommandManager.get();
    }

    bool Application::IsRenderThreadRunning()
    {
        return GetInstance()->m_RenderThreadRunning.load();
    }

    std::thread::id Application::GetMainThreadId()
    {
        return GetInstance()->m_MainThreadId;
    }

    ImGuiContext *Application::GetImGuiContext()
    {
        Application *app = GetInstance();
        if (!app->m_ImGuiLayer)
            return nullptr;

        return ImGui::GetCurrentContext();
    }

    float Application::GetDeltaTime()
    {
        return GetInstance()->m_DeltaTime;
    }

}
