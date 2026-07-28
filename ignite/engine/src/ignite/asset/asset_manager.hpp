// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_MANAGER_HPP
#define IGN_ASSET_MANAGER_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/subsystem.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include "asset_worker.hpp"

#include <map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>
#include <string_view>
#include <atomic>

namespace fbxsdk
{
    class FbxManager;
}

namespace ignite
{
    using AssetRegistry = std::map<AssetHandle, AssetMetaData>;

    class Project;

	class IGN_API AssetManager : public Subsystem
    {
    public:
        virtual void Init() override;
		virtual void Shutdown() override;

        void Reset();

        void SetActiveProject(const Ref<Project> &project);
        void SyncFromRust();

        Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata);

        AssetHandle ImportAsset(const ignite::Path &filepath);
        AssetHandle ImportAssetImmedate(const ignite::Path &filepath);

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
                }
            }
        }

        void RemoveAsset(AssetHandle handle);
        void LoadAssetAsync(AssetHandle handle);
        void LoadAssetImmediate(AssetHandle handle);

        void OnUpdate(float deltaTime);

        void OnAssetChangeSignal(const AssetChangeSignal &signal);

        template<typename T = Asset>
        Ref<T> GetAsset(AssetHandle handle)
        {
            if (!IsAssetHandleValid(handle))
            {
                return nullptr;
            }

            AssetMetaData metadata;
            // Protect all reads/writes to LoadedAssets and LoadingAssets
            {
                if (m_LoadedAssets.contains(handle))
                {
                    return std::static_pointer_cast<T>(m_LoadedAssets.at(handle));
                }

                if (m_LoadingAssets.contains(handle))
                {
                    return nullptr;
                }

                m_LoadingAssets.insert(handle);

                // Capture metadata while holding the lock so AssetRegistry read is safe
                if (m_AssetRegistry.contains(handle))
                {
                    metadata = m_AssetRegistry.at(handle);
                }
            }

            // Submit import work to worker thread
            AssetWorker::SubmitJob(std::format("Loading {}...", metadata.filepath.filename().string()), [this, handle, metadata]()
            {
                try
                {
                    // Do the heavy I/O work on worker thread
                    Ref<Asset> asset = Import(handle, metadata);
                    if (asset)
                    {
                        std::stringstream ss;
                        ss << std::this_thread::get_id();
                        unsigned long long threadId = std::stoull(ss.str());
                        LOG_TRACE("[Asset Manager] Asset loaded on worker thread [{0}]: {1} ({2})",
                            threadId, static_cast<uint64_t>(handle), metadata.filepath.generic_string());
                    }
                }
                catch (const std::exception &e)
                {
                    LOG_ASSERT(false, "[Asset Manager] Failed to import asset {} \"{}\": {}",
                        static_cast<uint64_t>(handle), metadata.filepath.generic_string(), e.what());
                }

                {
                    std::unique_lock lock(m_AssetMutex);
                    m_LoadingAssets.erase(handle);
                }
            });

            return nullptr;
        }
        
        template<typename T = Asset>
        Ref<T> GetAssetImmediate(AssetHandle handle)
        {
            VerifyNotRenderThread();

            if (!IsAssetHandleValid(handle))
            {
                return nullptr;
            }

            // Check if already loaded
            {
                std::unique_lock lock(m_AssetMutex);
                if (m_LoadedAssets.contains(handle))
                {
                    return std::static_pointer_cast<T>(m_LoadedAssets.at(handle));
                }
            }

            // Synchronous load - blocks calling thread
            const AssetMetaData metadata = GetMetaData(handle);
            LOG_TRACE("[Asset Manager] Synchronous asset load requested: {}", metadata.filepath.generic_string());

            Ref<Asset> asset = Import(handle, metadata);
            return std::static_pointer_cast<T>(asset);
        }

        AssetType GetAssetType(AssetHandle handle) const;

        const AssetMetaData &GetMetaData(const ignite::Path &filepath, AssetHandle &outHandle);
        const AssetMetaData &GetMetaData(AssetHandle handle) const;
        
        AssetHandle GetAssetHandle(const ignite::Path &filepath);
        
        const ignite::Path &GetFilepath(AssetHandle handle) const;
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

        WeakRef<Project> GetActiveProjectWeak() const { return m_Project; }

        Ref<Project> LockActiveProject() const { return m_Project.lock(); }

        static AssetManager *GetInstance();

        fbxsdk::FbxManager *GetOrCreateFbxSdkManager();
        std::mutex &GetFbxSdkMutex() { return m_FbxSdkMutex; }

        uint64_t GetAssetFileSize(const AssetMetaData &metadata) const;

    private:
        static void VerifyNotRenderThread();

        std::atomic<uint64_t> m_ActiveLoadBytes{ 0 };
        std::mutex m_ThrottleMutex;
        std::condition_variable m_ThrottleCV;

        AssetRegistry m_AssetRegistry;
        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
        std::unordered_set<AssetHandle> m_LoadingAssets;
        WeakRef<Project> m_Project;

        fbxsdk::FbxManager *m_FbxSdkManager = nullptr;
        std::atomic<bool> m_UnloadPaused = false;

        std::mutex m_FbxSdkMutex;
        mutable std::mutex m_AssetMutex;

        float assetUnloadTimer = 0.0f;
        
        std::unordered_map<std::string, AssetHandle> m_AssetHandleByPath;

        std::queue<std::function<bool()>> m_OnChangeCallbacks;

        SignalToken m_AssetChangeToken = kInvalidSignalToken;
        uint64_t m_LastSyncedRustVersion = 0;
    };
}

#endif
