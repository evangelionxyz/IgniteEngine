// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_manager.hpp"
#include "ignite/core/application.hpp"
#include "asset_importer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/base.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"

#include "ignite_rs/core.h"
#include <fbxsdk.h>

namespace ignite
{
    static AssetMetaData s_NullMetaData;
    static AssetManager *s_AssetManagerInstance = nullptr;

    void AssetManager::VerifyNotRenderThread()
    {
        auto *app = Application::GetInstance();
        const std::thread *renderThread = app ? app->GetRenderThread() : nullptr;
        if (renderThread && Application::IsRenderThreadRunning() && std::this_thread::get_id() == renderThread->get_id())
        {
            LOG_ASSERT(false, "[Asset Manager] GetAssetImmediate called on the Render Thread! This will cause stuttering and deadlocks!");
        }
    }

    uint64_t AssetManager::GetAssetFileSize(const AssetMetaData &metadata) const
    {
        if (metadata.filepath.empty())
        {
            return 0;
        }

        std::error_code ec;
        const auto absolutePath = LockActiveProject()->GetProjectFilepath(metadata.filepath);
        const uint64_t size = std::filesystem::file_size(absolutePath.string(), ec);
        return ec ? 0 : size;
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

    void AssetManager::Init()
    {
        LOG_WARN("[Asset Manager] Initialized");
        s_AssetManagerInstance = this;

        m_AssetChangeToken = SignalBus::Subscribe<AssetChangeSignal>(
            [this](const AssetChangeSignal &signal)
            {
                OnAssetChangeSignal(signal);
            });
    }

    void AssetManager::Shutdown()
    {
        SignalBus::Unsubscribe<AssetChangeSignal>(m_AssetChangeToken);
        m_AssetChangeToken = kInvalidSignalToken;
        if (m_FbxSdkManager)
        {
            m_FbxSdkManager->Destroy();
            m_FbxSdkManager = nullptr;
        }

        Reset();
        s_AssetManagerInstance = nullptr;

        LOG_WARN("[Asset Manager] Shutdown");
    }

    void AssetManager::Reset()
    {
        IGN_PROFILE_FUNCTION();

        if (auto *device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }

        decltype(m_LoadedAssets) OLD_LoadedAssets;
        decltype(m_PinnedAssetsByOwner) OLD_PinnedAssetsByOwner;
        decltype(m_AssetPinCounts) OLD_AssetPinCounts;
        decltype(m_LoadingAssets) OLD_LoadingAssets;
        decltype(m_AssetRegistry) OLD_AssetRegistry;
        {
            std::unique_lock lock(m_AssetMutex);
            OLD_LoadedAssets.swap(m_LoadedAssets);
            OLD_PinnedAssetsByOwner.swap(m_PinnedAssetsByOwner);
            OLD_AssetPinCounts.swap(m_AssetPinCounts);
            OLD_LoadingAssets.swap(m_LoadingAssets);
            OLD_AssetRegistry.swap(m_AssetRegistry);
        }

        OLD_LoadedAssets.clear();
        OLD_PinnedAssetsByOwner.clear();
        OLD_AssetPinCounts.clear();
        OLD_AssetRegistry.clear();

        m_Project.reset();
        m_AssetHandleByPath.clear();
        m_LastSyncedRustVersion = 0;
    }

    void AssetManager::SetActiveProject(const Ref<Project> &project)
    {
        if (m_Project.lock() == project)
            return;

        Reset();
        m_Project = project;
    }

    AssetManager *AssetManager::GetInstance()
    {
        return s_AssetManagerInstance;
    }

    fbxsdk::FbxManager *AssetManager::GetOrCreateFbxSdkManager()
    {
        if (!m_FbxSdkManager)
        {
            std::unique_lock lock(m_FbxSdkMutex);

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

    AssetHandle AssetManager::ImportAsset(const ignite::Path &filepath)
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
            metadata.filepath = filepath;
            metadata.type = GetAssetTypeFromExtension(filepath.extension().generic_string());
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

    AssetHandle AssetManager::ImportAssetImmedate(const ignite::Path &filepath)
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
            metadata.filepath = filepath;
            metadata.type = GetAssetTypeFromExtension(filepath.extension().generic_string());
            AssignMetaData(handle, metadata);
            GetAssetImmediate(handle);
        }
        else
        {
            // get the asset
            Ref<Asset> asset = GetAssetImmediate<Asset>(handle);
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
        LOG_ASSERT(handle != AssetHandle(0), "[Asset Manager] Invalid asset handle");

        std::unique_lock lock(m_AssetMutex);

        // Sync metadata with Rust AssetManager backend
        ignite_rs_asset_assign_metadata(static_cast<uint64_t>(handle),
                                        metadata.filepath.generic_string().c_str(), static_cast<AssetType_RS>(metadata.type));
    }

    const std::string AssetManager::GetAssetDisplayName(AssetHandle handle) const
    {
        if (!IsAssetHandleValid(handle))
            return "None";

        const AssetMetaData &metadata = GetMetaData(handle);
        if (!metadata.filepath.empty())
            return metadata.filepath.filename().string();
        return std::format("Handle {}", static_cast<uint64_t>(handle));
    }

    void AssetManager::RemoveAsset(AssetHandle handle)
    {
        IGN_PROFILE_FUNCTION();

        std::unique_lock lock(m_AssetMutex);

        // Sync removal with Rust AssetManager backend
        ignite_rs_asset_remove_metadata(static_cast<uint64_t>(handle));
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
            ignite_rs_asset_pin(static_cast<uint64_t>(handle));
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

        ignite_rs_asset_unpin(static_cast<uint64_t>(handle));
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

    uint32_t AssetManager::GetAssetPinCount(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);
        if (m_AssetPinCounts.contains(handle))
        {
            return m_AssetPinCounts.at(handle);
        }
        return 0;
    }

    std::vector<std::string> AssetManager::GetAssetPinOwners(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);
        std::vector<std::string> owners;
        for (const auto &[owner, assets] : m_PinnedAssetsByOwner)
        {
            if (assets.contains(handle))
            {
                owners.push_back(owner);
            }
        }
        return owners;
    }

    void AssetManager::SyncFromRust()
    {
        const uint64_t rustVersion = ignite_rs_asset_get_registry_version();
        if (rustVersion == m_LastSyncedRustVersion)
            return;

        const size_t count = ignite_rs_asset_get_registry_count();
        if (count == 0)
        {
            m_LastSyncedRustVersion = rustVersion;
            return;
        }

        std::vector<IgniteAssetRegistryEntryFFI> entries(count);
        const size_t fetched = ignite_rs_asset_get_registry_snapshot(entries.data(), count);

        std::unique_lock lock(m_AssetMutex);
        auto activeProject = LockActiveProject();
        for (size_t i = 0; i < fetched; ++i)
        {
            const AssetHandle handle = AssetHandle(entries[i].handle);
            if (handle == AssetHandle(0))
                continue;

            AssetMetaData metadata;
            metadata.filepath = entries[i].filepath;
            metadata.type = static_cast<AssetType>(entries[i].asset_type);

            m_AssetRegistry[handle] = metadata;

            if (!metadata.filepath.empty())
            {
                const ignite::Path absoluteMetadataPath = activeProject ? activeProject->GetProjectFilepath(metadata.filepath) : metadata.filepath;
                m_AssetHandleByPath[absoluteMetadataPath.generic_string()] = handle;
            }
        }

        m_LastSyncedRustVersion = rustVersion;
    }

    void AssetManager::OnUpdate(float deltaTime)
    {
        // Sync metadata registry from Rust source of truth
        SyncFromRust();

        // Call each frame
        assetUnloadTimer += deltaTime;
        constexpr float kUnloadInterval = 10.0f;
        if (assetUnloadTimer >= kUnloadInterval)
        {
            assetUnloadTimer = 0.0f;
            UnloadUnusedAssets();
        }

        // Call each frame
        if (!m_OnChangeCallbacks.empty())
        {
            auto &f = m_OnChangeCallbacks.front();
            if (f && f())
            {
                m_OnChangeCallbacks.pop();
            }
        }
    }

    void AssetManager::OnAssetChangeSignal(const AssetChangeSignal &signal)
    {
        auto activeScene = LockActiveProject()->LockActiveScene();
        if (!activeScene)
            return;

        switch (signal.type)
        {
            case AssetType::AnimatorController:
            {
                break;
            }
            case AssetType::Texture:
            case AssetType::Environment:
            case AssetType::Material:
            {
                auto onChangeFunc = [this, signal]() -> bool
                    {
                        const auto &assets = GetLoadedAssets();
                        for (const auto &[handle, asset] : assets)
                        {
                            if (asset && asset->GetAssetType() == AssetType::Material)
                            {
                                Ref<Material> material = std::static_pointer_cast<Material>(asset);
                                if (!material)
                                    continue;

                                if (signal.type == AssetType::Material)
                                {
                                    if (handle == signal.handle)
                                    {
                                        material->InvalidateBindingSet();
                                    }
                                }
                                else
                                {
                                    auto isTextureBeingUsed = [this](AssetHandle textureHandle, Ref<Material> material) -> bool
                                        {
                                            return textureHandle == material->baseColorTextureHandle || textureHandle == material->emissiveTextureHandle || textureHandle == material->metallicTextureHandle || textureHandle == material->roughnessTextureHandle || textureHandle == material->normalTextureHandle || textureHandle == material->occlusionTextureHandle;
                                        };

                                    const bool validTextureRequest = (signal.type == AssetType::Texture) && isTextureBeingUsed(signal.handle, material);
                                    if (signal.type == AssetType::Environment || validTextureRequest)
                                    {
                                        material->InvalidateBindingSet();
                                    }
                                }
                            }
                        }
                        return true;
                    };

                m_OnChangeCallbacks.push(std::move(onChangeFunc));
                break;
            }
        }
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
            // LOG_DEBUG("[Asset Manager] Unloading asset: {}", static_cast<uint64_t>(handle));

            // Release lock before GPU sync to avoid blocking other threads
            Ref<Asset> asset = it->second;
            m_LoadedAssets.erase(it);
            lock.unlock();

            // Wait for GPU to ensure asset is not in use (outside lock)
            if (auto *device = DeviceManager::GetInstance()->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }
        }
    }

    void AssetManager::UnloadUnusedAssets()
    {
        if (m_UnloadPaused)
        {
            return;
        }

        IGN_PROFILE_FUNCTION();

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
            auto *deviceManager = DeviceManager::GetInstance();
            if (deviceManager && deviceManager->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(deviceManager->GetDevice());
            }

            assetsToDestroy.clear();
            LOG_DEBUG("[Asset Manager] Unused assets unloaded. Remaining: {}", m_LoadedAssets.size());
        }
    }

    AssetType AssetManager::GetAssetType(AssetHandle handle) const
    {
        return GetMetaData(handle).type;
    }

    const AssetMetaData &AssetManager::GetMetaData(const ignite::Path &filepath, AssetHandle &outHandle)
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

    AssetHandle AssetManager::GetAssetHandle(const ignite::Path &filepath)
    {
        const auto &absoluteFilepath = std::filesystem::absolute(LockActiveProject()->GetProjectFilepath(filepath).string());

        std::unique_lock lock(m_AssetMutex);
        auto it = m_AssetHandleByPath.find(absoluteFilepath.generic_string());
        if (it != m_AssetHandleByPath.end())
        {
            return it->second;
        }

        return AssetHandle(0);
    }

    const ignite::Path &AssetManager::GetFilepath(AssetHandle handle) const
    {
        return GetMetaData(handle).filepath;
    }

    bool AssetManager::IsAssetHandleValid(AssetHandle handle) const
    {
        return static_cast<uint64_t>(handle) != 0 && m_AssetRegistry.contains(handle);
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

        static constexpr uint64_t MAX_CONCURRENT_LOAD_BYTES = 64 * 1024 * 1024; // 512 MB

        const uint64_t size = GetAssetFileSize(metadata);

        struct ThrottleGuard
        {
            AssetManager *manager;
            uint64_t size;

            ThrottleGuard(AssetManager *m, uint64_t s)
                : manager(m), size(s)
            {
                if (size > 0 && manager)
                {
                    std::unique_lock<std::mutex> lock(manager->m_ThrottleMutex);
                    manager->m_ThrottleCV.wait(lock, [&]()
                                               {
                        const bool signaled = manager->m_ActiveLoadBytes == 0 || (manager->m_ActiveLoadBytes + size <= MAX_CONCURRENT_LOAD_BYTES);
                        if (!signaled)
                        {
                            LOG_WARN("[Throttle Guard] Throttling asset loading {} bytes, max concurent load bytes: {} bytes",
                                manager->m_ActiveLoadBytes + size, MAX_CONCURRENT_LOAD_BYTES);
                        }
                        return signaled; });
                    manager->m_ActiveLoadBytes += size;
                }
            }

            ~ThrottleGuard()
            {
                if (size > 0 && manager)
                {
                    std::lock_guard<std::mutex> lock(manager->m_ThrottleMutex);
                    manager->m_ActiveLoadBytes -= size;
                    manager->m_ThrottleCV.notify_all();
                }
            }
        } guard(this, size);

        Ref<Asset> asset;

        bool isValidType = false;
        AssetMetaData getterMetadata = metadata;
        switch (getterMetadata.type)
        {
        case AssetType::Invalid:
        {
            LOG_ASSERT(false, "[Asset Manager] Invalid asset type!");
            return nullptr;
        }

        case AssetType::Audio:
        case AssetType::Mesh:
        case AssetType::StaticMesh:
        case AssetType::SkeletalMesh:
        case AssetType::Font:
        case AssetType::Material:
        case AssetType::Material2D:
        case AssetType::BlendSpace:
        case AssetType::Widget:
        case AssetType::Animation2D:
        case AssetType::SpriteSheet:
        case AssetType::SkeletalAnimation:
        case AssetType::AnimatorController:
        case AssetType::AnimatorController2D:
        case AssetType::ScriptableObject:
        case AssetType::Prefab:
        {
            isValidType = true;

            asset = AssetImporter::Import(handle, getterMetadata, this);
            {
                std::unique_lock lock(m_AssetMutex);
                if (m_LoadedAssets.contains(handle))
                {
                    isValidType = true;
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
            isValidType = true;

            if (getterMetadata.type == AssetType::Texture)
            {
                if (auto project = LockActiveProject())
                {
                    AssetMetaData textureMetadata = getterMetadata;
                    textureMetadata.filepath = project->GetProjectFilepath(getterMetadata.filepath);
                    TextureCreateInfo createInfo = Texture::GetDefaultCreateInfo(getterMetadata);
                    if (!Texture::LoadCreateInfoFile(Texture::GetMetaPath(project.get(), getterMetadata), createInfo))
                    {
                        Texture::LoadCreateInfoFile(Texture::GetLegacyMetaPath(project.get(), getterMetadata), createInfo);
                    }

                    asset = AssetImporter::ImportTexture(handle, textureMetadata, createInfo, this);
                }
            }
            else
            {
                asset = AssetImporter::Import(handle, getterMetadata, this);
            }

            {
                std::unique_lock lock(m_AssetMutex);
                if (m_LoadedAssets.contains(handle))
                {
                    isValidType = true;
                    return m_LoadedAssets[handle];
                }
            }
            AssignAsset(handle, asset);
            break;
        }
        }

        LOG_ASSERT(isValidType, "[Asset Manager] Failed to import asset, please check the AssetType: {}", getterMetadata.filepath.generic_string());
        return asset;
    }
}
