// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include "asset.hpp"
#include "ignite/core/logger.hpp"

#include <map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>
#include <string_view>

namespace fbxsdk
{
    class FbxManager;
}

namespace ignite
{

    using AssetRegistry = std::map<AssetHandle, AssetMetaData>;
	using AssetLoadedCallback = std::function<void(AssetHandle, AssetType)>;
    using AssetJob = std::function<void()>;

    class Project;

    class AssetManager
    {
    public:
        AssetManager(Project *project);
        ~AssetManager();

        Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata);
        AssetHandle ImportAsset(const std::filesystem::path &filepath);

        void AssignMetaData(AssetHandle handle, const AssetMetaData &metadata);

        const std::string GetAssetDisplayName(AssetHandle handle) const;

        template<typename T = Asset>
        void AssignAsset(AssetHandle handle, const Ref<T> &asset)
        {
            if (asset && std::is_base_of_v<Asset, T>)
            {
                m_LoadedAssets[handle] = asset;
                for (const auto &callback : m_LoadedCallbacks)
                {
                    callback(handle, asset->GetAssetType());
                }
            }
        }

        void RemoveAsset(AssetHandle handle);
        void LoadAssetAsync(AssetHandle handle);
        void LoadAssetImmediate(AssetHandle handle);

        void AddAssetPin(AssetHandle handle, std::string_view ownerTag);
        void RemoveAssetPin(AssetHandle handle, std::string_view ownerTag);
        void ReplaceAssetPins(const std::string &ownerTag, const std::unordered_set<AssetHandle> &handles);
        void ClearAssetPins(std::string_view ownerTag);
        bool IsAssetPinned(AssetHandle handle) const;
        
        // Register callback to be notified when assets are loaded
        void RegisterAssetLoadedCallback(AssetLoadedCallback callback)
        {
            m_LoadedCallbacks.push_back(callback);
        }

        void SubmitJob(AssetJob job);

        template<typename T = Asset>
        Ref<T> GetAsset(AssetHandle handle)
        {
            if (!IsAssetHandleValid(handle))
            {
                return nullptr;
            }

            // Quick check if already loaded
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
            }

            // Submit import work to worker thread
            const AssetMetaData metadata = GetMetaData(handle);

            SubmitJob([this, handle, metadata]()
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
                        LOG_TRACE("[Asset Manager] Asset loaded on worker thread [{0}]: {1} ({2})", threadId, static_cast<uint64_t>(handle), metadata.filepath.generic_string());
                    }
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR("[Asset Manager] Failed to import asset {} \"{}\": {}", static_cast<uint64_t>(handle), metadata.filepath.generic_string(), e.what());
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

        const AssetMetaData &GetMetaData(const std::filesystem::path &filepath, AssetHandle &outHandle);
        const AssetMetaData &GetMetaData(AssetHandle handle) const;
        
        AssetHandle GetAssetHandle(const std::filesystem::path &filepath);
        
        const std::filesystem::path &GetFilepath(AssetHandle handle) const;
        bool IsAssetHandleValid(AssetHandle handle) const;
        
        // Asset lifecycle management
        void ClearAllLoadedAssets();
        void UnloadAsset(AssetHandle handle);
        void UnloadUnusedAssets();
        size_t GetLoadedAssetCount() const { return m_LoadedAssets.size(); }
        bool IsAssetLoaded(AssetHandle handle) const;
        bool IsAssetLoading(AssetHandle handle) const;
        const std::unordered_map<AssetHandle, Ref<Asset>>& GetLoadedAssets() const { return m_LoadedAssets; }
    
        AssetRegistry &GetAssetAssetRegistry() { return m_AssetRegistry; }

        Project *GetProject() { return m_Project; }

        fbxsdk::FbxManager *GetOrCreateFbxSdkManager();
        std::mutex &GetFbxSdkMutex() { return m_FbxSdkMutex; }

    private:
        void WorkerLoop();

        AssetRegistry m_AssetRegistry;
        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
        std::unordered_set<AssetHandle> m_LoadingAssets;
        std::vector<AssetLoadedCallback> m_LoadedCallbacks;

        std::condition_variable m_ConditionVariable;
        std::vector<std::thread> m_Workers;
        std::queue<AssetJob> m_Jobs;
        Project *m_Project;

        fbxsdk::FbxManager *m_FbxSdkManager = nullptr;

        std::mutex m_FbxSdkMutex;
        std::mutex m_JobMutex;
        mutable std::mutex m_AssetMutex;
        
        bool m_Running;

        std::unordered_map<AssetHandle, uint32_t> m_AssetPinCounts;
        std::unordered_map<std::string, std::unordered_set<AssetHandle>> m_PinnedAssetsByOwner;
    };
}

#endif
