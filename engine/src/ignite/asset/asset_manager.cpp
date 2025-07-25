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

#include "ignite/core/logger.hpp"
#include <cstdint>

namespace ignite {

    static AssetMetaData s_NullMetaData;

    AssetManager::AssetManager()
        : m_Running(true)
    {
        const uint32_t THREAD_COUNT = std::max(std::thread::hardware_concurrency() / 2u, 1u);
        LOG_WARN("[Asset Manager] Creating {} worker threads!", THREAD_COUNT);

        for (uint32_t i = 0; i < THREAD_COUNT; ++i)
        {
            m_Workers.emplace_back(&AssetManager::WorkerLoop, this);
        }
    }

    AssetManager::~AssetManager()
    {
        {
            std::unique_lock lock(m_Mutex);
            m_Running = false;
        }

        // Notify other threads
        m_ConditionVariable.notify_all();
        for (std::thread &worker : m_Workers)
        {
            worker.join();
        }
    }

    AssetHandle AssetManager::ImportAsset(const std::filesystem::path &filepath)
    {
        bool foundInAssetRegistry = false;

        AssetHandle handle = AssetHandle(0);
        
        std::string fileExtension = filepath.extension().generic_string();

        // create metadata
        AssetMetaData metadata;

        // ixasset means it is part of the project
        // so we need to check from the asset registry
        if (fileExtension == ".ixasset")
        {
            metadata = GetMetaData(filepath, handle);
            foundInAssetRegistry = metadata.type != AssetType::Invalid && handle != AssetHandle(0);
        }

        if (!foundInAssetRegistry)
        {
            metadata.filepath = filepath;
            metadata.type = GetAssetTypeFromExtension(fileExtension);
        }

        // Invalid 
        if (metadata.type == AssetType::Invalid)
        {
            LOG_ERROR("[Asset Manager] Invalid asset type '{}'", filepath.generic_string());
            return AssetHandle(0);
        }

        // Find in registered asset first
        if (!foundInAssetRegistry)
        {
            for (const auto &[assetHandle, assetMetaData] : m_AssetRegistry)
            {
                if (metadata.filepath == assetMetaData.filepath)
                {
                    // found it
                    handle = assetHandle;
                    metadata = assetMetaData;

                    foundInAssetRegistry = true;
                    break;
                }
            }
        }

        // generate handle for new asset
        if (!foundInAssetRegistry)
        {
            handle = AssetHandle();
            Import(handle, metadata);
            m_AssetRegistry[handle] = metadata;
        }
        else
        {
            // get the asset
            Ref<Asset> asset = GetAsset(handle);
            if (!asset)
            {
                m_AssetRegistry[handle] = metadata;
                m_LoadedAssets[handle] = asset;
            }
        }


        return handle;
    }

    void AssetManager::InsertMetaData(AssetHandle handle, const AssetMetaData &metadata)
    {
        m_AssetRegistry[handle] = metadata;
    }

    void AssetManager::RemoveAsset(AssetHandle handle)
    {
        auto it = m_AssetRegistry.find(handle);
        if (it != m_AssetRegistry.end())
            m_AssetRegistry.erase(it);
    }

    void AssetManager::SubmitJob(AssetJob job)
    {
        {
            std::unique_lock lock(m_Mutex);
            m_Jobs.push(std::move(job));
        }

        m_ConditionVariable.notify_one();
    }

    Ref<Asset> AssetManager::GetAsset(AssetHandle handle)
    {
        if (!IsAssetHandleValid(handle))
        {
            return nullptr;
        }

        const AssetMetaData &metadata = GetMetaData(handle);
        return Import(handle, metadata);
    }

    AssetType AssetManager::GetAssetType(AssetHandle handle) const
    {
        return GetMetaData(handle).type;
    }

    const AssetMetaData &AssetManager::GetMetaData(const std::filesystem::path &filepath, AssetHandle &outHandle)
    {
        for (const auto &[handle, metadata] : m_AssetRegistry)
        {
            if (metadata.filepath == filepath)
            {
                // found it
                outHandle = handle;
                return metadata;
            }
        }
        return s_NullMetaData;
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
        for (const auto &[handle, metadata] : m_AssetRegistry)
        {
            if (metadata.filepath == filepath)
                return handle;
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

    void AssetManager::WorkerLoop()
    {
        while (true)
        {
            AssetJob job;
            
            {
                std::unique_lock lock(m_Mutex);
                m_ConditionVariable.wait(lock, [this]() { 
                    return !m_Running || !m_Jobs.empty(); 
                });

                // stop the loop if engine is shutting down
                if (!m_Running && m_Jobs.empty())
                    return;

                job = std::move(m_Jobs.front());
                m_Jobs.pop();
            }

            // Execute job
            job();
        }
    }

    Ref<Asset> AssetManager::Import(AssetHandle handle, const AssetMetaData &metadata)
    {
        if (m_LoadedAssets.contains(handle))
        {
            return m_LoadedAssets[handle];
        }

        Ref<Asset> asset;
        switch (metadata.type)
        {
        case AssetType::Invalid:
        {
            LOG_ERROR("[Asset Manager] Invalid asset type!");
            return nullptr;
        }

        case AssetType::MeshSource:
        case AssetType::Skeleton:
        case AssetType::Scene:
        case AssetType::Texture:
        {
            asset = AssetImporter::Import(handle, metadata);
            m_LoadedAssets[handle] = asset;
            asset->SetReadyFlag(true);
            break;
        }
        case AssetType::Audio:
        {
            m_LoadedAssets[handle] = asset;

            AssetImporter::ImportAsync(handle, metadata, [&](Ref<Asset> assetResult, AssetHandle assetHandle)
            {
                assetResult->SetReadyFlag(true);
                m_LoadedAssets[assetHandle] = assetResult;
            });

            break;
        }
        }

        return asset;
    }

}
