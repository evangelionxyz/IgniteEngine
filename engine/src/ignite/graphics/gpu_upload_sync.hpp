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

#include "ignite/core/application.hpp"

#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>

namespace ignite
{
    // Shared GPU upload synchronization to prevent Vulkan threading errors
    // These are accessed from both the engine (asset importer) and editor (content browser)
    class GPUUploadSync
    {
    public:
        static std::mutex &GetMutex()
        {
            static std::mutex s_Mutex;
            return s_Mutex;
        }

        static std::atomic<bool> &GetInProgressFlag()
        {
            static std::atomic<bool> s_InProgress{ false };
            return s_InProgress;
        }

        // Mutex specifically for waitForIdle() calls - Vulkan's waitForIdle is NOT thread-safe!
        static std::mutex &GetWaitIdleMutex()
        {
            static std::mutex s_WaitIdleMutex;
            return s_WaitIdleMutex;
        }

        // Thread-safe wrapper for device->waitForIdle()
        static void DeviceWaitIdle(nvrhi::IDevice *device)
        {
            std::lock_guard<std::mutex> lock(GetWaitIdleMutex());
            device->waitForIdle();

            if (Application::GetInstance()->GetRenderThread())
            {
                auto t = Application::GetInstance()->GetRenderThread();
                if (std::this_thread::get_id() != t->get_id())
                {
                    LOG_ERROR("[Wrong thread]");
                }
            }
        }

        // Helper to wait for any in-progress upload to complete
        static void WaitForCompletion()
        {
            while (GetInProgressFlag().load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // RAII lock guard for GPU uploads
        class ScopedLock
        {
        public:
            ScopedLock()
            {
                // Wait for any previous upload to complete
                WaitForCompletion();
                // Acquire the mutex
                GetMutex().lock();
                // Set the in-progress flag
                GetInProgressFlag() = true;
            }

            ~ScopedLock()
            {
                // Clear the in-progress flag
                GetInProgressFlag() = false;
                // Release the mutex
                GetMutex().unlock();
            }

            // Prevent copying
            ScopedLock(const ScopedLock &) = delete;
            ScopedLock &operator=(const ScopedLock &) = delete;
        };

        // Legacy helper functions (deprecated, use ScopedLock instead)
        static void BeginUpload()
        {
            WaitForCompletion();
            GetMutex().lock();
            GetInProgressFlag() = true;
        }

        static void EndUpload()
        {
            GetInProgressFlag() = false;
            GetMutex().unlock();
        }
    };
}
