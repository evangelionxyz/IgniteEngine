// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_worker.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/worker_manager.hpp"

namespace ignite
{
    static AssetWorker::StatusCallback s_StatusCallback;
    static std::string s_LastStatus = "Ready";
    static std::mutex s_StatusMutex;
    static std::atomic<int> s_ActiveJobs = 0;
    static std::atomic<int> s_TotalBatchJobs = 0;
    static std::atomic<int> s_CompletedBatchJobs = 0;

    void AssetWorker::Init()
    {
        WorkerManager::Get().Initialize();
        LOG_INFO("[Asset Worker] Using the shared WorkerManager asset pool");
    }

    void AssetWorker::SetStatusCallback(StatusCallback callback)
    {
        std::lock_guard<std::mutex> lock(s_StatusMutex);
        s_StatusCallback = std::move(callback);
    }

    void AssetWorker::ReportStatus(std::string_view status, float progress)
    {
        StatusCallback callback;
        {
            std::lock_guard<std::mutex> lock(s_StatusMutex);
            s_LastStatus.assign(status);
            callback = s_StatusCallback;
        }

        if (callback)
        {
            callback(status, progress);
        }
    }

    int AssetWorker::GetActiveJobCount()
    {
        return s_ActiveJobs.load(std::memory_order_acquire);
    }

    bool AssetWorker::IsBusy()
    {
        return GetActiveJobCount() > 0;
    }

    float AssetWorker::GetProgress()
    {
        const int active = s_ActiveJobs.load(std::memory_order_acquire);
        const int total = s_TotalBatchJobs.load(std::memory_order_acquire);
        const int completed = s_CompletedBatchJobs.load(std::memory_order_acquire);
        if (active <= 0 || total <= 0)
        {
            return 0.0f;
        }

        // Keep an active batch strictly inside (0, 1) so progress UI remains visible
        const float ratio = static_cast<float>(completed) / static_cast<float>(total);
        return std::clamp(ratio, 0.05f, 0.99f);
    }

    void AssetWorker::Shutdown()
    {
        std::lock_guard<std::mutex> lock(s_StatusMutex);
        s_StatusCallback = nullptr;
        s_LastStatus = "Ready";
    }

    void AssetWorker::SubmitJob(AssetJob assetJob)
    {
        IGN_PROFILE_FUNCTION();

        if (!assetJob)
        {
            return;
        }

        if (s_ActiveJobs.fetch_add(1, std::memory_order_acq_rel) == 0)
        {
            s_TotalBatchJobs.store(1, std::memory_order_release);
            s_CompletedBatchJobs.store(0, std::memory_order_release);
        }
        else
        {
            s_TotalBatchJobs.fetch_add(1, std::memory_order_acq_rel);
        }

        const bool queued = WorkerManager::Get().GetAssetPool().EnqueueTask(
            [assetJob = std::move(assetJob)]() mutable
            {
                try
                {
                    IGN_PROFILE_SCOPE_COLOR("AssetWorker::Execute", 0x00AABCFF);
                    assetJob();
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR("[Asset Worker] Job failed: {}", e.what());
                }
                catch (...)
                {
                    LOG_ERROR("[Asset Worker] Job failed with an unknown error");
                }

                s_CompletedBatchJobs.fetch_add(1, std::memory_order_acq_rel);
                if (s_ActiveJobs.fetch_sub(1, std::memory_order_acq_rel) - 1 == 0)
                {
                    s_TotalBatchJobs.store(0, std::memory_order_release);
                    s_CompletedBatchJobs.store(0, std::memory_order_release);
                    AssetWorker::ReportStatus("Ready", 0.0f);
                }
                else
                {
                    std::string currentStatus;
                    {
                        std::lock_guard<std::mutex> lock(s_StatusMutex);
                        currentStatus = s_LastStatus;
                    }
                    AssetWorker::ReportStatus(currentStatus, AssetWorker::GetProgress());
                }
            });

        if (!queued)
        {
            s_CompletedBatchJobs.fetch_add(1, std::memory_order_acq_rel);
            if (s_ActiveJobs.fetch_sub(1, std::memory_order_acq_rel) - 1 == 0)
            {
                s_TotalBatchJobs.store(0, std::memory_order_release);
                s_CompletedBatchJobs.store(0, std::memory_order_release);
                ReportStatus("Ready", 0.0f);
            }
        }
    }

    void AssetWorker::SubmitJob(std::string_view name, AssetJob assetJob)
    {
        SubmitJob([nameStr = std::string(name), assetJob = std::move(assetJob)]() mutable
        {
            ReportStatus(nameStr, GetProgress());
            assetJob();
        });
    }

    void AssetWorker::SubmitBackgroundJob(AssetJob assetJob)
    {
        IGN_PROFILE_FUNCTION();

        if (!assetJob)
        {
            return;
        }

        WorkerManager::Get().GetAssetPool().EnqueueTask(
            [assetJob = std::move(assetJob)]() mutable
            {
                try
                {
                    IGN_PROFILE_SCOPE_COLOR("AssetWorker::Background", 0x00AABCFF);
                    assetJob();
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR("[Asset Worker] Background job failed: {}", e.what());
                }
                catch (...)
                {
                    LOG_ERROR("[Asset Worker] Background job failed with an unknown error");
                }
            });
    }
}
