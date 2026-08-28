// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_worker.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/worker_manager.hpp"

namespace ignite
{
    static AssetWorker::StatusCallback s_StatusCallback;
    static std::mutex s_StatusMutex;
    static std::atomic<int> s_ActiveJobs = 0;

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
            callback = s_StatusCallback;
        }

        if (callback)
        {
            callback(status, progress);
        }
    }

    void AssetWorker::Shutdown()
    {
        std::lock_guard<std::mutex> lock(s_StatusMutex);
        s_StatusCallback = nullptr;
    }

    void AssetWorker::SubmitJob(AssetJob assetJob)
    {
        IGN_PROFILE_FUNCTION();

        if (!assetJob)
        {
            return;
        }

        ++s_ActiveJobs;
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

                if (--s_ActiveJobs == 0)
                {
                    AssetWorker::ReportStatus("Ready", 0.0f);
                }
            });

        if (!queued && --s_ActiveJobs == 0)
        {
            ReportStatus("Ready", 0.0f);
        }
    }

    void AssetWorker::SubmitJob(std::string_view name, AssetJob assetJob)
    {
        SubmitJob([nameStr = std::string(name), assetJob = std::move(assetJob)]() mutable
        {
            ReportStatus(nameStr);
            assetJob();
        });
    }
}
