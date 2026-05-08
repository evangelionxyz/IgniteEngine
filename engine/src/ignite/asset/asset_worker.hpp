// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ASSET_WORKER_HPP
#define ASSET_WORKER_HPP

#include <functional>
#include <string_view>

namespace ignite
{
    using AssetJob = std::function<void()>;

    class AssetWorker
    {
    public:
        static void Init();
        static void Shutdown();

        static void SubmitJob(AssetJob assetJob);
        static void SubmitJob(std::string_view name, AssetJob assetJob);
        
        using StatusCallback = std::function<void(std::string_view, float)>;
        static void SetStatusCallback(StatusCallback callback);
        static void ReportStatus(std::string_view status, float progress = -1.0f);

    private:
        static void WorkerLoop();
    };
}

#endif