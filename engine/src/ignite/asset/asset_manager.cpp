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

#include "asset_manager.hpp"
#include "asset_importer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include "ignite/core/logger.hpp"
#include <cstdint>

#include <fbxsdk.h>

namespace ignite {

	static std::mutex s_AssetThreadMutex;
	static AssetMetaData s_NullMetaData;

    static Project *s_Project = nullptr;

    AssetManager::AssetManager(Project *project)
        : m_Running(true)
    {
        s_Project = project;

        const uint32_t THREAD_COUNT = std::max(std::thread::hardware_concurrency() / 2u, 1u);
        LOG_WARN("[Asset Manager] Creating {} worker threads!", THREAD_COUNT);

        for (uint32_t i = 0; i < THREAD_COUNT; ++i)
        {
            m_Workers.emplace_back(&AssetManager::WorkerLoop, this);
        }

		for (uint32_t i = 0; i < THREAD_COUNT; ++i)
        {
            std::stringstream ss;
            ss << m_Workers[i].get_id();
            unsigned long long id = std::stoull(ss.str());
            LOG_WARN("[Asset Manager] Worker [{0}]: {1}", i, id);
        }
    }

    bool AssetManager::IsAssetLoaded(AssetHandle handle) const
    {
        std::unique_lock lock(s_AssetThreadMutex);

        const auto it = m_LoadedAssets.find(handle);
        if (it == m_LoadedAssets.end() || !it->second)
        {
            return false;
        }

        return it->second->IsReady();
    }

    bool AssetManager::IsAssetLoading(AssetHandle handle) const
    {
        std::unique_lock lock(s_AssetThreadMutex);

        if (m_LoadingAssets.contains(handle))
        {
            return true;
        }

        const auto it = m_LoadedAssets.find(handle);
        if (it == m_LoadedAssets.end() || !it->second)
        {
            return false;
        }

        return !it->second->IsReady();
    }

    AssetManager::~AssetManager()
    {
        {
            std::unique_lock lock(s_AssetThreadMutex);
            m_Running = false;
        }

        // Notify other threads
        m_ConditionVariable.notify_all();
        for (std::thread &worker : m_Workers)
        {
            worker.join();
        }

        if (m_FbxSdkManager)
        {
            m_FbxSdkManager->Destroy();
            m_FbxSdkManager = nullptr;
        }
    }

    fbxsdk::FbxManager *AssetManager::GetOrCreateFbxSdkManager()
    {
        if (!m_FbxSdkManager)
        {
            m_FbxSdkManager = fbxsdk::FbxManager::Create();
            if (!m_FbxSdkManager)
            {
                return nullptr;
            }

            fbxsdk::FbxIOSettings *ioSettings = fbxsdk::FbxIOSettings::Create(m_FbxSdkManager, IOSROOT);
            m_FbxSdkManager->SetIOSettings(ioSettings);
        }

        return m_FbxSdkManager;
    }

    AssetHandle AssetManager::ImportAsset(const std::filesystem::path &filepath)
    {
        // Invalid 
        if (GetAssetTypeFromExtension(filepath.extension().generic_string()) == AssetType::Invalid)
        {
            LOG_ERROR("[Asset Manager] Invalid asset type '{}'", filepath.generic_string());
            return AssetHandle(0);
        }

        // Find in registered asset first
        AssetHandle handle = AssetHandle(0);
        AssetMetaData metadata = GetMetaData(filepath, handle);

        // generate handle for new asset
        if (handle == AssetHandle(0))
        {
            handle = AssetHandle();
            Import(handle, metadata);
            AssignMetaData(handle, metadata);
        }
        else
        {
            // get the asset
            Ref<Asset> asset = GetAsset(handle);
            if (!asset)
            {
                AssignMetaData(handle, metadata);
                AssignAsset(handle, asset);
            }
        }

        return handle;
    }

    void AssetManager::AssignMetaData(AssetHandle handle, const AssetMetaData &metadata)
    {
        m_AssetRegistry[handle] = metadata;
    }

	void AssetManager::RemoveAsset(AssetHandle handle)
    {
        auto it = m_AssetRegistry.find(handle);
        if (it != m_AssetRegistry.end())
            m_AssetRegistry.erase(it);
    }

    void AssetManager::ClearAllLoadedAssets()
    {
        LOG_TRACE("[Asset Manager] Clearing all loaded assets (Count: {})", m_LoadedAssets.size());
        
        // Wait for GPU operations to complete before releasing assets
        if (auto* device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }
        
        {
            std::unique_lock lock(s_AssetThreadMutex);
            m_LoadedAssets.clear();
        }
        
        LOG_TRACE("[Asset Manager] All loaded assets cleared");
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        std::unique_lock lock(s_AssetThreadMutex);
        
        auto it = m_LoadedAssets.find(handle);
        if (it != m_LoadedAssets.end())
        {
            LOG_TRACE("[Asset Manager] Unloading asset: {}", static_cast<uint64_t>(handle));
            
            // Release lock before GPU sync to avoid blocking other threads
            Ref<Asset> asset = it->second;
            m_LoadedAssets.erase(it);
            lock.unlock();
            
            // Wait for GPU to ensure asset is not in use (outside lock)
            if (auto* device = DeviceManager::GetInstance()->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }

            // asset will be destroyed here when going out of scope
        }
    }

    void AssetManager::UnloadUnusedAssets()
    {
        LOG_TRACE("[Asset Manager] Checking for unused assets (Loaded: {})", m_LoadedAssets.size());

        std::vector<AssetHandle> assetsToUnload;
        std::vector<Ref<Asset>> assetsToDestroy;

        // Find assets that are only referenced by m_LoadedAssets (use_count == 1)
        {
            std::unique_lock lock(s_AssetThreadMutex);

            for (const auto &[handle, asset] : m_LoadedAssets)
            {
                if (asset && asset.use_count() == 1)
                {
                    assetsToUnload.push_back(handle);
                    assetsToDestroy.push_back(asset);
                }
            }

            if (!assetsToUnload.empty())
            {
                LOG_TRACE("[Asset Manager] Unloading {} unused assets", assetsToUnload.size());

                for (AssetHandle handle : assetsToUnload)
                {
                    m_LoadedAssets.erase(handle);
                }
            }
        }

        // GPU sync and asset destruction outside lock
        if (!assetsToUnload.empty())
        {
            // Wait for GPU to ensure assets are not in use
            if (auto *device = DeviceManager::GetInstance()->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }
            
            assetsToDestroy.clear();
            
            LOG_TRACE("[Asset Manager] Unused assets unloaded. Remaining: {}", m_LoadedAssets.size());
        }
        else
        {
            LOG_TRACE("[Asset Manager] No unused assets found");
        }
    }

    void AssetManager::SubmitJob(AssetJob job)
    {
        {
            std::unique_lock lock(s_AssetThreadMutex);
            m_Jobs.push(std::move(job));
        }

        m_ConditionVariable.notify_one();
    }

    Ref<Asset> AssetManager::GetAsset(AssetHandle handle, AssetType requestedAssetType)
    {
        if (!IsAssetHandleValid(handle))
        {
            return nullptr;
        }

        // Quick check if already loaded (no lock needed for read)
        {
            std::unique_lock lock(s_AssetThreadMutex);
            
            // Return immediately if loaded
            if (m_LoadedAssets.contains(handle))
            {
                return m_LoadedAssets[handle];
            }
            
            // Check if already loading to avoid duplicate work
            if (m_LoadingAssets.contains(handle))
            {
                // Asset is being loaded on another thread, return nullptr for now
                // Caller should check IsAssetLoaded() or retry later
                return nullptr;
            }
            
            // Mark as loading
            m_LoadingAssets.insert(handle);
        }

        // Submit import work to worker thread
        const AssetMetaData metadata = GetMetaData(handle);
        
        SubmitJob([this, handle, metadata, requestedAssetType]()
        {
            try
            {
                // Do the heavy I/O work on worker thread
                Ref<Asset> asset = Import(handle, metadata, requestedAssetType);
                
                if (asset)
                {
					std::stringstream ss;
					ss << std::this_thread::get_id();
					unsigned long long threadId = std::stoull(ss.str());
                    LOG_TRACE("[Asset Manager] Asset loaded on worker thread [{0}]: {1} ({2})",
                        threadId,
                        static_cast<uint64_t>(handle), 
                        metadata.filepath.generic_string());
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("[Asset Manager] Failed to import asset {} \"{}\": {}", 
                    static_cast<uint64_t>(handle), metadata.filepath.generic_string(), e.what());
            }
            
            // Remove from loading set
            {
                std::unique_lock lock(s_AssetThreadMutex);
                m_LoadingAssets.erase(handle);
            }
        });
        
        // Return nullptr immediately - asset will be loaded asynchronously
        // Caller should check IsAssetLoaded() or use a callback pattern
        return nullptr;
    }

    Ref<Asset> AssetManager::GetAssetImmediate(AssetHandle handle, AssetType requestedAssetType)
    {
        if (!IsAssetHandleValid(handle))
        {
            return nullptr;
        }

        // Check if already loaded
        {
            std::unique_lock lock(s_AssetThreadMutex);
            if (m_LoadedAssets.contains(handle))
            {
                return m_LoadedAssets[handle];
            }
        }

        // Synchronous load - blocks calling thread
        const AssetMetaData metadata = GetMetaData(handle);
        LOG_TRACE("[Asset Manager] Synchronous asset load requested: {}", 
            metadata.filepath.generic_string());
        
        return Import(handle, metadata, requestedAssetType);
    }

    AssetType AssetManager::GetAssetType(AssetHandle handle) const
    {
        return GetMetaData(handle).type;
    }

    const AssetMetaData &AssetManager::GetMetaData(const std::filesystem::path &filepath, AssetHandle &outHandle)
    {
        outHandle = GetAssetHandle(filepath);
        return m_AssetRegistry.at(outHandle);;
    }

    const AssetMetaData &AssetManager::GetMetaData(AssetHandle handle) const
    {
        if (m_AssetRegistry.contains(handle))
        {
            return m_AssetRegistry.at(handle);
        }
        return s_NullMetaData;
    }

    AssetHandle AssetManager::GetAssetHandle(const std::filesystem::path& filepath)
    {
        // Normalize the input filepath to absolute path for comparison
        std::filesystem::path absoluteFilepath = std::filesystem::absolute(s_Project->GetAssetFilepath(filepath));

        for (const auto &[handle, metadata] : m_AssetRegistry)
        {
            // Convert metadata filepath (relative) to absolute using project base path
            std::filesystem::path absoluteMetadataPath = s_Project 
                ? std::filesystem::absolute(s_Project->GetAssetFilepath(metadata.filepath))
                : std::filesystem::absolute(metadata.filepath);
            
            // Compare normalized absolute paths
            if (absoluteFilepath == absoluteMetadataPath)
            {
                return handle;
            }
        }

        return AssetHandle(0);
    }

    const std::filesystem::path &AssetManager::GetFilepath(AssetHandle handle) const
    {
        return GetMetaData(handle).filepath;
    }

    bool AssetManager::IsAssetHandleValid(AssetHandle handle) const
    {
        return static_cast<uint64_t>(handle) != 0 && m_AssetRegistry.contains(handle);
    }

	Project *AssetManager::GetProject()
    {
        return s_Project;
    }

	void AssetManager::WorkerLoop()
    {
        while (true)
        {
            AssetJob job;
            
            {
                std::unique_lock lock(s_AssetThreadMutex);
                m_ConditionVariable.wait(lock, [this]() { return !m_Running || !m_Jobs.empty(); });

                // stop the loop if engine is shutting down
                if (!m_Running && m_Jobs.empty())
                {
                    return;
                }

                job = std::move(m_Jobs.front());
                m_Jobs.pop();
            }

            // Execute job outside lock with exception handling
            try
            {
                job();
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("[Asset Manager] Worker thread exception: {}", e.what());
            }
            catch (...)
            {
                LOG_ERROR("[Asset Manager] Worker thread exception: unknown error");
            }
        }
    }

    Ref<Asset> AssetManager::Import(AssetHandle handle, const AssetMetaData &metadata, AssetType requestedAssetType)
    {
        // Check if already loaded (thread-safe read)
        {
            std::unique_lock lock(s_AssetThreadMutex);
            if (m_LoadedAssets.contains(handle))
            {
                return m_LoadedAssets[handle];
            }
        }

        Ref<Asset> asset;

        AssetMetaData getterMetadata = metadata;
        if (requestedAssetType != AssetType::Auto && requestedAssetType != AssetType::Invalid)
        {
            getterMetadata.type = requestedAssetType;
        }

        switch (getterMetadata.type)
        {
        case AssetType::Invalid:
        {
            LOG_ERROR("[Asset Manager] Invalid asset type!");
            return nullptr;
        }

        case AssetType::Material:
        case AssetType::Material2D:
        case AssetType::StaticMesh:
        case AssetType::SkeletalAnimation:
        case AssetType::SkeletalMesh:
        {
            asset = AssetImporter::Import(handle, getterMetadata);
            
            // Thread-safe assignment
            {
                std::unique_lock lock(s_AssetThreadMutex);
                // Double-check if another thread loaded it while we were importing
                if (m_LoadedAssets.contains(handle))
                {
                    return m_LoadedAssets[handle];
                }
                AssignAsset(handle, asset);
            }
            break;
        }
        case AssetType::Skeleton:
        case AssetType::Scene:
        case AssetType::Texture:
        {
            asset = AssetImporter::Import(handle, getterMetadata);
            
            // Thread-safe assignment
            {
                std::unique_lock lock(s_AssetThreadMutex);
                // Double-check if another thread loaded it while we were importing
                if (m_LoadedAssets.contains(handle))
                {
                    return m_LoadedAssets[handle];
                }
                AssignAsset(handle, asset);
            }
            break;
        }
        case AssetType::Audio:
        {
            {
                std::unique_lock lock(s_AssetThreadMutex);
                AssignAsset(handle, asset);
            }
            AssetImporter::ImportAsync(handle, metadata, [&](Ref<Asset> assetResult, AssetHandle assetHandle)
            {
                assetResult->SetReadyFlag(true);
                std::unique_lock lock(s_AssetThreadMutex);
                AssignAsset(assetHandle, assetResult);
            });
            break;
        }
        }

        if (asset && asset->GetAssetType() != AssetType::Texture)
        {
            asset->SetReadyFlag(true);
        }

        return asset;
    }

}
