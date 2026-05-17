// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef GPU_UPLOAD_SYNC_HPP
#define GPU_UPLOAD_SYNC_HPP

#include <mutex>
#include <atomic>

namespace nvrhi
{
    class IDevice;
}

namespace ignite
{
    // Shared GPU upload synchronization to prevent Vulkan threading errors
    // These are accessed from both the engine (asset importer) and editor (content browser)
    class GPUUploadSync
    {
    public:
        static std::mutex &GetMutex();
        static std::mutex &GetQueueMutex();
        static std::atomic<bool> &GetInProgressFlag();

        // Mutex specifically for waitForIdle() calls - Vulkan's waitForIdle is NOT thread-safe!
        static std::mutex &GetWaitIdleMutex();

        // Thread-safe wrapper for device->waitForIdle()
        static void DeviceWaitIdle(nvrhi::IDevice *device);

        // Helper to wait for any in-progress upload to complete
        static void WaitForCompletion();

        // RAII lock guard for GPU uploads
        class ScopedLock
        {
        public:
            ScopedLock();
            ~ScopedLock();

            // Prevent copying
            ScopedLock(const ScopedLock &) = delete;
            ScopedLock &operator=(const ScopedLock &) = delete;
        };

        // Legacy helper functions (deprecated, use ScopedLock instead)
        static void BeginUpload();
        static void EndUpload();
    };
}

#endif