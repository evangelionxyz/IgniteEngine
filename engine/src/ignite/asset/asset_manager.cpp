// Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO

#include "asset_manager.hpp"
#include "asset_importer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/base.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/texture.hpp"
#include <cstdint>

#include <fbxsdk.h>

namespace ignite {

    static AssetMetaData s_NullMetaData;

    AssetManager::AssetManager(Project *project)
        : m_Running(true), m_Project(project)
    {
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
        std::unique_lock lock(m_AssetMutex);

        const auto it = m_LoadedAssets.find(handle);
        if (it == m_LoadedAssets.end() || !it->second)
        {
            return false;
        }

        return it->second->IsReady();
    }

    bool AssetManager::IsAssetLoading(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);

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
            std::unique_lock lock(m_JobMutex);
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
        IGN_PROFILE_FUNCTION();

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
            AssignMetaData(handle, metadata);
            GetAsset(handle);
        }
        else
        {
            // get the asset
            Ref<Asset> asset = GetAsset<Asset>(handle);
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

    const std::string AssetManager::GetAssetDisplayName(AssetHandle handle) const
    {
        if (handle == AssetHandle(0))
        {
            return "None";
        }

        const AssetMetaData &metadata = GetMetaData(handle);
        if (!metadata.filepath.empty())
        {
            return metadata.filepath.filename().string();
        }

        return std::format("Handle {}", static_cast<uint64_t>(handle));
    }

    void AssetManager::RemoveAsset(AssetHandle handle)
    {
        IGN_PROFILE_FUNCTION();
        
        std::unique_lock lock(m_AssetMutex);

        auto it = m_AssetRegistry.find(handle);
        if (it != m_AssetRegistry.end())
            m_AssetRegistry.erase(it);
    }

    void AssetManager::LoadAssetAsync(AssetHandle handle)
    {
        GetAsset<Asset>(handle);
    }

    void AssetManager::LoadAssetImmediate(AssetHandle handle)
    {
        GetAssetImmediate<Asset>(handle);
    }

    void AssetManager::AddAssetPin(AssetHandle handle, std::string_view ownerTag)
    {
        if (handle == AssetHandle(0) || ownerTag.empty())
        {
            return;
        }

        std::unique_lock lock(m_AssetMutex);
        auto &ownedAssets = m_PinnedAssetsByOwner[std::string(ownerTag)];
        if (ownedAssets.insert(handle).second)
        {
            ++m_AssetPinCounts[handle];
        }
    }

    void AssetManager::RemoveAssetPin(AssetHandle handle, std::string_view ownerTag)
    {
        if (handle == AssetHandle(0) || ownerTag.empty())
        {
            return;
        }

        std::unique_lock lock(m_AssetMutex);
        const auto ownerIt = m_PinnedAssetsByOwner.find(std::string(ownerTag));
        if (ownerIt == m_PinnedAssetsByOwner.end())
        {
            return;
        }

        if (!ownerIt->second.erase(handle))
        {
            return;
        }

        if (ownerIt->second.empty())
        {
            m_PinnedAssetsByOwner.erase(ownerIt);
        }

        const auto pinIt = m_AssetPinCounts.find(handle);
        if (pinIt == m_AssetPinCounts.end())
        {
            return;
        }

        if (pinIt->second <= 1)
        {
            m_AssetPinCounts.erase(pinIt);
        }
        else
        {
            --pinIt->second;
        }
    }

    void AssetManager::ReplaceAssetPins(const std::string &ownerTag, const std::unordered_set<AssetHandle> &handles)
    {
        if (ownerTag.empty())
        {
            return;
        }

        std::unique_lock lock(m_AssetMutex);
        std::unordered_set<AssetHandle> previousHandles;
        if (const auto ownerIt = m_PinnedAssetsByOwner.find(ownerTag); ownerIt != m_PinnedAssetsByOwner.end())
        {
            previousHandles = ownerIt->second;
        }

        for (AssetHandle handle : previousHandles)
        {
            if (handles.contains(handle))
            {
                continue;
            }

            if (auto pinIt = m_AssetPinCounts.find(handle); pinIt != m_AssetPinCounts.end())
            {
                if (pinIt->second <= 1)
                {
                    m_AssetPinCounts.erase(pinIt);
                }
                else
                {
                    --pinIt->second;
                }
            }
        }

        for (AssetHandle handle : handles)
        {
            if (handle == AssetHandle(0) || previousHandles.contains(handle))
            {
                continue;
            }

            ++m_AssetPinCounts[handle];
        }

        if (handles.empty())
        {
            m_PinnedAssetsByOwner.erase(ownerTag);
        }
        else
        {
            m_PinnedAssetsByOwner[ownerTag] = handles;
        }
    }

    void AssetManager::ClearAssetPins(std::string_view ownerTag)
    {
        ReplaceAssetPins(std::string(ownerTag), {});
    }

    bool AssetManager::IsAssetPinned(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);
        return m_AssetPinCounts.contains(handle);
    }

    void AssetManager::ClearAllLoadedAssets()
    {
        IGN_PROFILE_FUNCTION();

        LOG_DEBUG("[Asset Manager] Clearing all loaded assets (Count: {})", m_LoadedAssets.size());
        
        if (auto* device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }
        
        {
            std::unique_lock lock(m_AssetMutex);
            m_LoadedAssets.clear();
        }
        
        LOG_DEBUG("[Asset Manager] All loaded assets cleared");
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        IGN_PROFILE_FUNCTION();

        if (auto *device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }

        std::unique_lock lock(m_AssetMutex);
        
        auto it = m_LoadedAssets.find(handle);
        if (it != m_LoadedAssets.end())
        {
            LOG_DEBUG("[Asset Manager] Unloading asset: {}", static_cast<uint64_t>(handle));
            
            // Release lock before GPU sync to avoid blocking other threads
            Ref<Asset> asset = it->second;
            m_LoadedAssets.erase(it);
            lock.unlock();
            
            // Wait for GPU to ensure asset is not in use (outside lock)
            if (auto* device = DeviceManager::GetInstance()->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }
        }
    }

    void AssetManager::UnloadUnusedAssets()
    {
        IGN_PROFILE_FUNCTION();

        LOG_DEBUG("[Asset Manager] Checking for unused assets (Loaded: {})", m_LoadedAssets.size());

        std::vector<AssetHandle> assetsToUnload;
        std::vector<Ref<Asset>> assetsToDestroy;

        {
            std::unique_lock lock(m_AssetMutex);

            for (const auto &[handle, asset] : m_LoadedAssets)
            {
                const bool pinned = m_AssetPinCounts.contains(handle);
                if (asset && !pinned && asset.use_count() == 1)
                {
                    assetsToUnload.push_back(handle);
                    assetsToDestroy.push_back(asset);
                }
            }

            if (!assetsToUnload.empty())
            {
                LOG_DEBUG("[Asset Manager] Unloading {} unused assets", assetsToUnload.size());

                for (AssetHandle handle : assetsToUnload)
                {
                    m_LoadedAssets.erase(handle);

                    LOG_DEBUG("[Asset Manager]    \"{}\" unloaded", GetAssetDisplayName(handle));
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
            
            LOG_DEBUG("[Asset Manager] Unused assets unloaded. Remaining: {}", m_LoadedAssets.size());
        }
        else
        {
            LOG_DEBUG("[Asset Manager] No unused assets found");
        }
    }

    void AssetManager::SubmitJob(AssetJob job)
    {
        IGN_PROFILE_FUNCTION();

        {
            std::unique_lock lock(m_JobMutex);
            m_Jobs.push(std::move(job));
        }

        m_ConditionVariable.notify_one();
    }
    
    AssetType AssetManager::GetAssetType(AssetHandle handle) const
    {
        return GetMetaData(handle).type;
    }

    const AssetMetaData &AssetManager::GetMetaData(const std::filesystem::path &filepath, AssetHandle &outHandle)
    {
        outHandle = GetAssetHandle(filepath);
        if (m_AssetRegistry.contains(outHandle))
        {
            return m_AssetRegistry.at(outHandle);
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

    AssetHandle AssetManager::GetAssetHandle(const std::filesystem::path &filepath)
    {
        std::filesystem::path absoluteFilepath = std::filesystem::absolute(m_Project->GetProjectFilepath(filepath));

        std::unique_lock lock(m_AssetMutex);
     
        for (const auto &[handle, metadata] : m_AssetRegistry)
        {
            std::filesystem::path absoluteMetadataPath = m_Project->GetProjectFilepath(metadata.filepath);
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

    void AssetManager::WorkerLoop()
    {
        while (true)
        {
            AssetJob job;
            
            {
                IGN_PROFILE_SCOPE("AssetManager::WorkerLoop");

                std::unique_lock lock(m_JobMutex);
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
                IGN_PROFILE_SCOPE_COLOR("AssetManager::WorkerLoop::Execute", 0x00AABCFF);
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

    Ref<Asset> AssetManager::Import(AssetHandle handle, const AssetMetaData &metadata)
    {
        IGN_PROFILE_FUNCTION();

        {
            std::unique_lock lock(m_AssetMutex);
            if (m_LoadedAssets.contains(handle))
            {
                return m_LoadedAssets[handle];
            }
        }

        Ref<Asset> asset;

        AssetMetaData getterMetadata = metadata;
        switch (getterMetadata.type)
        {
            case AssetType::Invalid:
            {
                LOG_ERROR("[Asset Manager] Invalid asset type!");
                return nullptr;
            }

            case AssetType::Audio:
            case AssetType::Mesh:
            case AssetType::Font:
            case AssetType::Material:
            case AssetType::Material2D:
            case AssetType::Widget:
            case AssetType::Animation2D:
            case AssetType::SpriteSheet:
            case AssetType::SkeletalAnimation:
            case AssetType::AnimatorController:
            case AssetType::AnimatorController2D:
            {
                asset = AssetImporter::Import(handle, getterMetadata, this);
                {
                    std::unique_lock lock(m_AssetMutex);
                    if (m_LoadedAssets.contains(handle))
                    {
                        return m_LoadedAssets[handle];
                    }
                }
                AssignAsset(handle, asset);
                break;
            }
            case AssetType::Skeleton:
            case AssetType::Scene:
            case AssetType::Texture:
            {
                if (getterMetadata.type == AssetType::Texture)
                {
                    AssetMetaData textureMetadata = getterMetadata;
                    textureMetadata.filepath = m_Project->GetProjectFilepath(getterMetadata.filepath);
                    TextureCreateInfo createInfo = Texture::GetDefaultCreateInfo(getterMetadata);
                    if (!Texture::LoadCreateInfoFile(Texture::GetMetaPath(m_Project, getterMetadata), createInfo))
                    {
                        Texture::LoadCreateInfoFile(Texture::GetLegacyMetaPath(m_Project, getterMetadata), createInfo);
                    }
                    asset = AssetImporter::ImportTexture(handle, textureMetadata, createInfo, this);
                }
                else
                {
                    asset = AssetImporter::Import(handle, getterMetadata, this);
                }

                {
                    std::unique_lock lock(m_AssetMutex);
                    if (m_LoadedAssets.contains(handle))
                    {
                        return m_LoadedAssets[handle];
                    }
                }
                AssignAsset(handle, asset);
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
