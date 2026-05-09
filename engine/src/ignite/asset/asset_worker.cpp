// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_worker.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>

namespace ignite
{
    struct AssetWorkerImpl
    {
        std::condition_variable conditionalVar;
        std::vector<std::thread> workers;
        std::queue<AssetJob> jobs;
        std::mutex jobMutex;
        bool running = false;
    };

    static AssetWorkerImpl s_AssetWorker;
    static AssetWorker::StatusCallback s_StatusCallback = nullptr;
    static std::atomic<int> s_ActiveJobs = 0;

    void AssetWorker::Init()
    {
        const uint32_t THREAD_COUNT = std::max(std::thread::hardware_concurrency() / 2u, 1u);
        LOG_WARN("[Asset Manager] Creating {} worker threads!", THREAD_COUNT);

        {
            std::unique_lock lock(s_AssetWorker.jobMutex);
            s_AssetWorker.running = true;
        }

        for (uint32_t i = 0; i < THREAD_COUNT; ++i)
        {
            s_AssetWorker.workers.emplace_back(&AssetWorker::WorkerLoop);
        }

        for (uint32_t i = 0; i < THREAD_COUNT; ++i)
        {
            std::stringstream ss;
            ss << s_AssetWorker.workers[i].get_id();
            unsigned long long id = std::stoull(ss.str());
            LOG_WARN("[Asset Manager] Worker [{0}]: {1}", i, id);
        }
    }
    
    void AssetWorker::SetStatusCallback(StatusCallback callback)
    {
        s_StatusCallback = callback;
    }

    void AssetWorker::ReportStatus(std::string_view status, float progress)
    {
        if (s_StatusCallback)
            s_StatusCallback(status, progress);
    }

    void AssetWorker::Shutdown()
    {
        {
            std::unique_lock lock(s_AssetWorker.jobMutex);
            s_AssetWorker.running = false;
        }

        // Notify other threads
        s_AssetWorker.conditionalVar.notify_all();
        for (std::thread &worker : s_AssetWorker.workers)
            worker.join();

        s_AssetWorker.workers.clear();
    }

    void AssetWorker::SubmitJob(AssetJob assetJob)
    {
        IGN_PROFILE_FUNCTION();
        s_ActiveJobs++;
        {
            std::unique_lock lock(s_AssetWorker.jobMutex);
            s_AssetWorker.jobs.push(std::move(assetJob));
        }

        s_AssetWorker.conditionalVar.notify_one();
    }

    void AssetWorker::SubmitJob(std::string_view name, AssetJob assetJob)
    {
        SubmitJob([nameStr = std::string(name), assetJob = std::move(assetJob)]()
        {
            ReportStatus(nameStr);
            assetJob();
        });
    }

    void AssetWorker::WorkerLoop()
    {
        while (true)
        {
            AssetJob job;

            {
                IGN_PROFILE_SCOPE("AssetManager::WorkerLoop");

                std::unique_lock lock(s_AssetWorker.jobMutex);
                s_AssetWorker.conditionalVar.wait(lock, [&]() { return !s_AssetWorker.running || !s_AssetWorker.jobs.empty(); });

                // stop the loop if engine is shutting down
                if (!s_AssetWorker.running && s_AssetWorker.jobs.empty())
                {
                    return;
                }

                job = std::move(s_AssetWorker.jobs.front());
                s_AssetWorker.jobs.pop();
            }

            // Execute job outside lock with exception handling
            try
            {
                IGN_PROFILE_SCOPE_COLOR("AssetManager::WorkerLoop::Execute", 0x00AABCFF);
                job();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("[Asset Manager] Worker thread exception: {}", e.what());
            }
            catch (...)
            {
                LOG_ERROR("[Asset Manager] Worker thread exception: unknown error");
            }

            if (--s_ActiveJobs == 0)
            {
                AssetWorker::ReportStatus("Ready", 0.0f);
            }
        }
    }
}
