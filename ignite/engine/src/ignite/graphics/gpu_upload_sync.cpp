// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "gpu_upload_sync.hpp"
#include "ignite/core/application.hpp"

namespace ignite
{
    std::mutex &GPUUploadSync::GetMutex()
    {
        static std::mutex s_Mutex;
        return s_Mutex;
    }

    std::mutex &GPUUploadSync::GetQueueMutex()
    {
        static std::mutex s_QueueMutex;
        return s_QueueMutex;
    }

    std::atomic<bool> &GPUUploadSync::GetInProgressFlag()
    {
        static std::atomic<bool> s_InProgress{ false };
        return s_InProgress;
    }

    std::mutex &GPUUploadSync::GetWaitIdleMutex()
    {
        static std::mutex s_WaitIdleMutex;
        return s_WaitIdleMutex;
    }

    void GPUUploadSync::DeviceWaitIdle(nvrhi::IDevice *device)
    {
        auto *app = Application::GetInstance();
        const std::thread *renderThread = app ? app->GetRenderThread() : nullptr;
        if (renderThread && Application::IsRenderThreadRunning() && std::this_thread::get_id() != renderThread->get_id())
        {
            std::mutex waitMutex;
            std::condition_variable waitCv;
            bool done = false;

            Application::SubmitToRenderThread([&]()
                {
                    {
                        std::scoped_lock lock(GetWaitIdleMutex(), GetQueueMutex());
                        device->waitForIdle();
                    }
                    {
                        std::lock_guard<std::mutex> guard(waitMutex);
                        done = true;
                    }
                    waitCv.notify_one();
                });

            std::unique_lock<std::mutex> waitLock(waitMutex);
            waitCv.wait(waitLock, [&]() { return done; });
            return;
        }

        const std::thread::id mainThreadId = Application::GetMainThreadId();
        if (!Application::IsRenderThreadRunning() && std::this_thread::get_id() != mainThreadId)
        {
            std::mutex waitMutex;
            std::condition_variable waitCv;
            bool done = false;

            Application::SubmitToMainThread([&]()
            {
                {
                    std::scoped_lock lock(GetWaitIdleMutex(), GetQueueMutex());
                    device->waitForIdle();
                }
                {
                    std::lock_guard<std::mutex> guard(waitMutex);
                    done = true;
                }
                waitCv.notify_one();
            });

            std::unique_lock<std::mutex> waitLock(waitMutex);
            waitCv.wait(waitLock, [&]() { return done; });
            return;
        }

        std::scoped_lock lock(GetWaitIdleMutex(), GetQueueMutex());
        device->waitForIdle();
    }

    void GPUUploadSync::WaitForCompletion()
    {
        while (GetInProgressFlag().load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    GPUUploadSync::ScopedLock::ScopedLock()
    {
        // Wait for any previous upload to complete
        WaitForCompletion();
        // Acquire the mutex
        GetMutex().lock();
        // Set the in-progress flag
        GetInProgressFlag() = true;
    }

    GPUUploadSync::ScopedLock::~ScopedLock()
    {
        // Clear the in-progress flag
        GetInProgressFlag() = false;
        // Release the mutex
        GetMutex().unlock();
    }

    void GPUUploadSync::BeginUpload()
    {
        WaitForCompletion();
        GetMutex().lock();
        GetInProgressFlag() = true;
    }

    void GPUUploadSync::EndUpload()
    {
        GetInProgressFlag() = false;
        GetMutex().unlock();
    }
}
