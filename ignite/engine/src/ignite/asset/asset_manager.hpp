// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_MANAGER_HPP
#define IGN_ASSET_MANAGER_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/subsystem.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"

#include <map>
#include <memory>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>
#include <string_view>
#include <atomic>

#include <cppcoro/async_scope.hpp>
#include <cppcoro/shared_task.hpp>

namespace fbxsdk
{
    class FbxManager;
}

namespace ignite
{
    class Project;

    using AssetRegistry = std::map<AssetHandle, AssetMetaData>;

    struct AssetStatus
    {
        AssetState state = AssetState::Unloaded;
        float progress = 0.0f;
        std::string error;
    };

    struct AssetRecord
    {
        AssetMetaData metadata;
        Ref<Asset> asset;
        std::atomic<AssetState> state{AssetState::Unloaded};
        std::string error;
        float progress = 0.0f;
        std::atomic<std::uint64_t> generation{ 0 };
        cppcoro::shared_task<Ref<Asset>> loadTask;
    };


	class IGN_API AssetManager : public Subsystem
    {
    public:
        virtual void Init() override;
		virtual void Shutdown() override;

        void Reset();

        void SetActiveProject(const Ref<Project> &project);
        void SyncFromRust();

        Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata, bool cacheResult = true);

        AssetHandle ImportAsset(const std::filesystem::path &filepath);
        AssetHandle ImportAssetImmedate(const std::filesystem::path &filepath);

        void AssignMetaData(AssetHandle handle, const AssetMetaData &metadata);

        const std::string GetAssetDisplayName(AssetHandle handle) const;

        template<typename T = Asset>
        void AssignAsset(AssetHandle handle, const Ref<T> &asset)
        {
			LOG_ASSERT(handle != AssetHandle(0), "[Asset Manager] Invalid asset handle");

            if (asset && std::is_base_of_v<Asset, T>)
            {
                Ref<Asset> oldAsset;
                {
                    std::unique_lock lock(m_AssetMutex);
                    if (m_LoadedAssets.contains(handle))
                    {
                        oldAsset = m_LoadedAssets.at(handle);
                    }
                    m_LoadedAssets[handle] = asset;

                    auto &record = m_AssetRecords[handle];
                    if (!record)
                    {
                        record = std::make_shared<AssetRecord>();
                        if (const auto metadataIt = m_AssetRegistry.find(handle); metadataIt != m_AssetRegistry.end())
                        {
                            record->metadata = metadataIt->second;
                        }
                    }

                    record->asset = asset;
                    record->error.clear();
                    record->progress = 1.0f;
                    record->state.store(AssetState::Ready, std::memory_order_release);
                }
            }
        }

        void RemoveAsset(AssetHandle handle);
        void LoadAssetAsync(AssetHandle handle);
        void LoadAssetImmediate(AssetHandle handle);

        cppcoro::shared_task<Ref<Asset>> RequestAssetAsync(AssetHandle handle);
        Ref<Asset> TryGetAsset(AssetHandle handle) const;
        AssetState GetAssetState(AssetHandle handle) const;
        AssetStatus GetAssetStatus(AssetHandle handle) const;
        bool RetryAsset(AssetHandle handle);
        void CancelAssetLoad(AssetHandle handle);

        void OnUpdate(float deltaTime);

        void OnAssetChangeSignal(const AssetChangeSignal &signal);

        template<typename T = Asset>
        Ref<T> GetAsset(AssetHandle handle)
        {
            if (Ref<Asset> asset = TryGetAsset(handle))
            {
                return std::static_pointer_cast<T>(asset);
            }

            (void)RequestAssetAsync(handle);
            return nullptr;
        }

        template<typename T = Asset>
        Ref<T> GetAssetImmediate(AssetHandle handle)
        {
            return std::static_pointer_cast<T>(GetAssetImmediateInternal(handle));
        }

        AssetType GetAssetType(AssetHandle handle) const;

        const AssetMetaData &GetMetaData(const std::filesystem::path &filepath, AssetHandle &outHandle);
        const AssetMetaData &GetMetaData(AssetHandle handle) const;

        AssetHandle GetAssetHandle(const std::filesystem::path &filepath);

        const std::filesystem::path &GetFilepath(AssetHandle handle) const;
        bool IsAssetHandleValid(AssetHandle handle) const;

        // Asset lifecycle management
        void UnloadAsset(AssetHandle handle);
        void UnloadUnusedAssets();
        void PauseUnloadAssets() { m_UnloadPaused = true; }
        void ResumeUnloadAssets() { m_UnloadPaused = false; }
        size_t GetLoadedAssetCount() const { return m_LoadedAssets.size(); }
        bool IsAssetLoaded(AssetHandle handle) const;
        bool IsAssetLoading(AssetHandle handle) const;
        const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() const { return m_LoadedAssets; }

        AssetRegistry &GetAssetAssetRegistry() { return m_AssetRegistry; }

        WeakRef<Project> GetActiveProjectWeak() const
        {
            std::lock_guard<std::mutex> lock(m_ProjectMutex);
            return m_Project;
        }

        Ref<Project> LockActiveProject() const
        {
            std::lock_guard<std::mutex> lock(m_ProjectMutex);
            return m_Project.lock();
        }

        static AssetManager *GetInstance();

        fbxsdk::FbxManager *GetOrCreateFbxSdkManager();
        std::mutex &GetFbxSdkMutex() { return m_FbxSdkMutex; }

        uint64_t GetAssetFileSize(const AssetMetaData &metadata) const;

    private:
        static void VerifyNotRenderThread();

        Ref<Asset> GetAssetImmediateInternal(AssetHandle handle);
        cppcoro::shared_task<Ref<Asset>> LoadAssetPipeline(AssetHandle handle, AssetMetaData metadata,
            Ref<AssetRecord> record, std::uint64_t managerGeneration, std::uint64_t requestGeneration);
        bool IsRequestCurrent(const Ref<AssetRecord> &record, std::uint64_t managerGeneration, std::uint64_t requestGeneration) const;
        void SetRecordState(const Ref<AssetRecord> &record, AssetState state, float progress, std::string error = {});

        std::atomic<uint64_t> m_ActiveLoadBytes{ 0 };
        std::mutex m_ThrottleMutex;
        std::condition_variable m_ThrottleCV;

        AssetRegistry m_AssetRegistry;
        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
        std::unordered_map<AssetHandle, Ref<AssetRecord>> m_AssetRecords;
        WeakRef<Project> m_Project;

        cppcoro::async_scope m_LoadScope;
        std::mutex m_LoadScopeMutex;
        std::atomic<std::uint64_t> m_Generation{ 1 };
        std::atomic<bool> m_ShuttingDown{ false };

        fbxsdk::FbxManager *m_FbxSdkManager = nullptr;
        std::atomic<bool> m_UnloadPaused = false;

        std::mutex m_FbxSdkMutex;
        mutable std::mutex m_AssetMutex;
        mutable std::mutex m_ProjectMutex;

        float assetUnloadTimer = 0.0f;

        std::unordered_map<std::string, AssetHandle> m_AssetHandleByPath;

        std::queue<std::function<bool()>> m_OnChangeCallbacks;

        SignalToken m_AssetChangeToken = kInvalidSignalToken;
        uint64_t m_LastSyncedRustVersion = 0;
    };
}

#endif
