// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_manager.hpp"
#include "ignite/core/application.hpp"
#include "asset_importer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/base.hpp"
#include "ignite/core/worker_manager.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/terrain/terrain.hpp"

#include "ignite_rs/core.h"

#include <cppcoro/sync_wait.hpp>
#include <fbxsdk.h>
#include <stdexcept>

namespace ignite
{
    static AssetMetaData s_NullMetaData;
    static AssetManager *s_AssetManagerInstance = nullptr;

    static cppcoro::shared_task<Ref<Asset>> MakeCompletedAssetTask(Ref<Asset> asset)
    {
        co_return asset;
    }

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

        if (auto project = LockActiveProject())
        {
            std::error_code ec;
            const auto absolutePath = project->GetProjectFilepath(metadata.filepath);
            const uint64_t size = std::filesystem::file_size(absolutePath.string(), ec);
            return ec ? 0 : size;    
        }

        return 0;
    }

    bool AssetManager::IsAssetLoaded(AssetHandle handle) const
    {
        return GetAssetState(handle) == AssetState::Ready;
    }

    bool AssetManager::IsAssetLoading(AssetHandle handle) const
    {
        const AssetState state = GetAssetState(handle);
        return state == AssetState::Queued ||
            state == AssetState::Loading ||
            state == AssetState::Finalizing;
    }

    void AssetManager::Init()
    {
        LOG_WARN("[Asset Manager] Initialized");
        m_ShuttingDown.store(false, std::memory_order_release);
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

        {
            std::lock_guard<std::mutex> scopeLock(m_LoadScopeMutex);
            m_ShuttingDown.store(true, std::memory_order_release);
            m_Generation.fetch_add(1, std::memory_order_acq_rel);
            cppcoro::sync_wait(m_LoadScope.join());
        }

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

        decltype(m_LoadedAssets) oldLoadedAssets;
        decltype(m_AssetRecords) oldAssetRecords;
        decltype(m_AssetRegistry) oldAssetRegistry;
        {
            std::unique_lock lock(m_AssetMutex);
            m_Generation.fetch_add(1, std::memory_order_acq_rel);
            oldLoadedAssets.swap(m_LoadedAssets);
            oldAssetRecords.swap(m_AssetRecords);
            oldAssetRegistry.swap(m_AssetRegistry);
            m_AssetHandleByPath.clear();
        }

        oldLoadedAssets.clear();
        oldAssetRegistry.clear();

        {
            std::lock_guard<std::mutex> projectLock(m_ProjectMutex);
            m_Project.reset();
        }
        m_LastSyncedRustVersion = 0;
    }

    void AssetManager::SetActiveProject(const Ref<Project> &project)
    {
        if (LockActiveProject() == project)
            return;

        Reset();
        std::lock_guard<std::mutex> projectLock(m_ProjectMutex);
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
            Ref<Project> activeProj = LockActiveProject();
            metadata.filepath = activeProj ? activeProj->GetProjectRelativeFilepath(filepath) : filepath;
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

    AssetHandle AssetManager::ImportAssetImmedate(const std::filesystem::path &filepath)
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
            Ref<Project> activeProj = LockActiveProject();
            metadata.filepath = activeProj ? activeProj->GetProjectRelativeFilepath(filepath) : filepath;
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

        {
            std::unique_lock lock(m_AssetMutex);

            // Sync metadata with Rust AssetManager backend
            ignite_rs_asset_assign_metadata(static_cast<uint64_t>(handle),
                metadata.filepath.generic_string().c_str(), static_cast<AssetType_RS>(metadata.type));
        }

        SyncFromRust();
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

        {
            std::unique_lock lock(m_AssetMutex);

            // Sync removal with Rust AssetManager backend
            ignite_rs_asset_remove_metadata(static_cast<uint64_t>(handle));
        }

        SyncFromRust();
    }

    void AssetManager::LoadAssetAsync(AssetHandle handle)
    {
        (void)RequestAssetAsync(handle);
    }

    void AssetManager::LoadAssetImmediate(AssetHandle handle)
    {
        (void)GetAssetImmediateInternal(handle);
    }

    cppcoro::shared_task<Ref<Asset>> AssetManager::RequestAssetAsync(AssetHandle handle)
    {
        std::lock_guard<std::mutex> scopeLock(m_LoadScopeMutex);
        if (m_ShuttingDown.load(std::memory_order_acquire) || handle == AssetHandle(0))
        {
            return MakeCompletedAssetTask(nullptr);
        }

        cppcoro::shared_task<Ref<Asset>> task;
        {
            std::unique_lock lock(m_AssetMutex);

            if (const auto loadedIt = m_LoadedAssets.find(handle);
                loadedIt != m_LoadedAssets.end() && loadedIt->second)
            {
                return MakeCompletedAssetTask(loadedIt->second);
            }

            const auto metadataIt = m_AssetRegistry.find(handle);
            if (metadataIt == m_AssetRegistry.end())
            {
                return MakeCompletedAssetTask(nullptr);
            }

            auto &record = m_AssetRecords[handle];
            if (!record)
            {
                record = std::make_shared<AssetRecord>();
            }
            record->metadata = metadataIt->second;

            const AssetState state = record->state.load(std::memory_order_acquire);
            if (state == AssetState::Queued || state == AssetState::Loading ||
                state == AssetState::Finalizing)
            {
                return record->loadTask;
            }
            if (state == AssetState::Failed)
            {
                return MakeCompletedAssetTask(nullptr);
            }

            record->asset.reset();
            record->error.clear();
            record->progress = 0.0f;
            record->state.store(AssetState::Queued, std::memory_order_release);

            const std::uint64_t managerGeneration = m_Generation.load(std::memory_order_acquire);
            const std::uint64_t requestGeneration = ++record->generation;
            task = LoadAssetPipeline(handle, record->metadata, record, managerGeneration, requestGeneration);
            record->loadTask = task;
        }

        m_LoadScope.spawn(task);
        return task;
    }

    Ref<Asset> AssetManager::TryGetAsset(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);
        const auto it = m_LoadedAssets.find(handle);
        return it != m_LoadedAssets.end() ? it->second : nullptr;
    }

    AssetState AssetManager::GetAssetState(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);
        if (const auto it = m_AssetRecords.find(handle); it != m_AssetRecords.end() && it->second)
        {
            return it->second->state.load(std::memory_order_acquire);
        }
        return m_LoadedAssets.contains(handle) ? AssetState::Ready : AssetState::Unloaded;
    }

    AssetStatus AssetManager::GetAssetStatus(AssetHandle handle) const
    {
        std::unique_lock lock(m_AssetMutex);
        if (const auto it = m_AssetRecords.find(handle); it != m_AssetRecords.end() && it->second)
        {
            return AssetStatus{
                it->second->state.load(std::memory_order_acquire),
                it->second->progress,
                it->second->error
            };
        }

        return AssetStatus{
            m_LoadedAssets.contains(handle) ? AssetState::Ready : AssetState::Unloaded,
            m_LoadedAssets.contains(handle) ? 1.0f : 0.0f,
            {}
        };
    }

    bool AssetManager::RetryAsset(AssetHandle handle)
    {
        {
            std::unique_lock lock(m_AssetMutex);
            const auto it = m_AssetRecords.find(handle);
            if (it == m_AssetRecords.end() || !it->second ||
                it->second->state.load(std::memory_order_acquire) != AssetState::Failed)
            {
                return false;
            }

            ++it->second->generation;
            it->second->loadTask = {};
            it->second->state.store(AssetState::Unloaded, std::memory_order_release);
        }

        (void)RequestAssetAsync(handle);
        return true;
    }

    void AssetManager::CancelAssetLoad(AssetHandle handle)
    {
        std::unique_lock lock(m_AssetMutex);
        const auto it = m_AssetRecords.find(handle);
        if (it == m_AssetRecords.end() || !it->second)
        {
            return;
        }

        const AssetState state = it->second->state.load(std::memory_order_acquire);
        if (state == AssetState::Queued || state == AssetState::Loading || state == AssetState::Finalizing)
        {
            ++it->second->generation;
            it->second->loadTask = {};
            it->second->asset.reset();
            it->second->error.clear();
            it->second->progress = 0.0f;
            it->second->state.store(AssetState::Unloaded, std::memory_order_release);
        }
    }

    Ref<Asset> AssetManager::GetAssetImmediateInternal(AssetHandle handle)
    {
        VerifyNotRenderThread();

        if (Ref<Asset> loaded = TryGetAsset(handle))
        {
            return loaded;
        }

        AssetMetaData metadata;
        Ref<AssetRecord> record;
        {
            std::unique_lock lock(m_AssetMutex);
            const auto metadataIt = m_AssetRegistry.find(handle);
            if (metadataIt == m_AssetRegistry.end())
            {
                return nullptr;
            }

            metadata = metadataIt->second;
            auto &entry = m_AssetRecords[handle];
            if (!entry)
            {
                entry = std::make_shared<AssetRecord>();
            }
            record = entry;
            record->metadata = metadata;
            ++record->generation;
            record->error.clear();
            record->progress = 0.1f;
            record->state.store(AssetState::Loading, std::memory_order_release);
        }

        LOG_TRACE("[Asset Manager] Synchronous asset load requested: {}", metadata.filepath.generic_string());
        try
        {
            Ref<Asset> asset = Import(handle, metadata);
            if (!asset)
            {
                throw std::runtime_error("Importer returned no asset");
            }
            return asset;
        }
        catch (const std::exception &e)
        {
            SetRecordState(record, AssetState::Failed, 0.0f, e.what());
            LOG_ERROR("[Asset Manager] Failed to load asset {} \"{}\": {}",
                static_cast<uint64_t>(handle), metadata.filepath.generic_string(), e.what());
        }
        catch (...)
        {
            SetRecordState(record, AssetState::Failed, 0.0f, "Unknown import error");
            LOG_ERROR("[Asset Manager] Failed to load asset {} \"{}\"",
                static_cast<uint64_t>(handle), metadata.filepath.generic_string());
        }

        return nullptr;
    }

    cppcoro::shared_task<Ref<Asset>> AssetManager::LoadAssetPipeline(AssetHandle handle, AssetMetaData metadata,
        Ref<AssetRecord> record, std::uint64_t managerGeneration, std::uint64_t requestGeneration)
    {
        try
        {
            co_await WorkerManager::Get().GetIOPool().Schedule();
            if (!IsRequestCurrent(record, managerGeneration, requestGeneration))
            {
                co_return nullptr;
            }

            SetRecordState(record, AssetState::Loading, 0.1f);

            // Importers currently combine file I/O, CPU decoding and resource
            // creation. Run that legacy stage on the asset pool until each
            // importer is split into explicit decode and finalization phases.
            co_await WorkerManager::Get().GetAssetPool().Schedule();
            if (!IsRequestCurrent(record, managerGeneration, requestGeneration))
            {
                co_return nullptr;
            }

            Ref<Asset> asset = Import(handle, metadata, false);
            if (!asset)
            {
                throw std::runtime_error("Importer returned no asset");
            }

            if (!IsRequestCurrent(record, managerGeneration, requestGeneration))
            {
                co_return nullptr;
            }

            SetRecordState(record, AssetState::Finalizing, 0.9f);
            AssignAsset(handle, asset);

            LOG_TRACE("[Asset Manager] Asset ready: {} ({})", static_cast<uint64_t>(handle), metadata.filepath.generic_string());
            co_return asset;
        }
        catch (const std::exception &e)
        {
            if (IsRequestCurrent(record, managerGeneration, requestGeneration))
            {
                SetRecordState(record, AssetState::Failed, 0.0f, e.what());

                LOG_ERROR("[Asset Manager] Failed to load asset {} \"{}\": {}",
                    static_cast<uint64_t>(handle), metadata.filepath.generic_string(), e.what());
            }
        }
        catch (...)
        {
            if (IsRequestCurrent(record, managerGeneration, requestGeneration))
            {
                SetRecordState(record, AssetState::Failed, 0.0f, "Unknown import error");

                LOG_ERROR("[Asset Manager] Failed to load asset {} \"{}\"",
                    static_cast<uint64_t>(handle), metadata.filepath.generic_string());
            }
        }

        co_return nullptr;
    }

    bool AssetManager::IsRequestCurrent(const Ref<AssetRecord> &record,
        std::uint64_t managerGeneration, std::uint64_t requestGeneration) const
    {
        return record && !m_ShuttingDown.load(std::memory_order_acquire) &&
            m_Generation.load(std::memory_order_acquire) == managerGeneration &&
            record->generation.load(std::memory_order_acquire) == requestGeneration;
    }

    void AssetManager::SetRecordState(const Ref<AssetRecord> &record, AssetState state, float progress, std::string error)
    {
        if (!record)
        {
            return;
        }

        std::unique_lock lock(m_AssetMutex);
        record->progress = progress;
        record->error = std::move(error);
        record->state.store(state, std::memory_order_release);
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

            // Asset Registry
            m_AssetRegistry[handle] = metadata;

            auto &record = m_AssetRecords[handle];
            if (!record)
            {
                record = std::make_shared<AssetRecord>();
            }
            record->metadata = metadata;

            if (!metadata.filepath.empty())
            {
                // Asset Handle by Path
                const std::filesystem::path absoluteMetadataPath = activeProject ? activeProject->GetProjectFilepath(metadata.filepath) : metadata.filepath;
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
            case AssetType::Terrain:
            {
                auto onChangeFunc = [this, signal]() -> bool
                {
                    auto activeScene = LockActiveProject()->LockActiveScene();
                    if (!activeScene || !activeScene->registry)
                        return true;

                    Ref<TerrainData> terrainAsset = GetAsset<TerrainData>(signal.handle);

                    auto terrainView = activeScene->registry->view<TerrainComponent>();
                    for (auto entity : terrainView)
                    {
                        auto &comp = terrainView.get<TerrainComponent>(entity);
                        if (comp.heightmapHandle == signal.handle || (comp.data && comp.data->handle == signal.handle))
                        {
                            if (terrainAsset)
                            {
                                comp.data = terrainAsset;
                                comp.resolution = terrainAsset->resolution;
                                comp.worldSize = terrainAsset->worldSize;
                                comp.maxHeight = terrainAsset->maxHeight;
                            }
                            comp.ReleaseGPU();
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

        std::unique_lock lock(m_AssetMutex);

        auto it = m_LoadedAssets.find(handle);
        if (it != m_LoadedAssets.end())
        {
            Ref<AssetRecord> record;
            if (const auto recordIt = m_AssetRecords.find(handle); recordIt != m_AssetRecords.end())
            {
                record = recordIt->second;
                ++record->generation;
                record->state.store(AssetState::Unloading, std::memory_order_release);
            }

            // Release lock before GPU sync to avoid blocking other threads
            Ref<Asset> asset = it->second;
            m_LoadedAssets.erase(it);
            lock.unlock();

            // Wait for GPU to ensure asset is not in use (outside lock)
            if (auto *device = DeviceManager::GetInstance()->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }

            if (record)
            {
                std::unique_lock recordLock(m_AssetMutex);
                record->asset.reset();
                record->progress = 0.0f;
                record->state.store(AssetState::Unloaded, std::memory_order_release);
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
                if (asset && asset.use_count() == 1)
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
                    if (const auto recordIt = m_AssetRecords.find(handle);
                        recordIt != m_AssetRecords.end() && recordIt->second)
                    {
                        ++recordIt->second->generation;
                        recordIt->second->asset.reset();
                        recordIt->second->progress = 0.0f;
                        recordIt->second->state.store(AssetState::Unloaded, std::memory_order_release);
                    }
                    LOG_DEBUG("[Asset Manager]    \"{}\" unloaded", GetAssetDisplayName(handle));
                }
            }
        }

        // GPU sync and asset destruction outside lock
        if (!assetsToUnload.empty())
        {
            if (auto *deviceManager = DeviceManager::GetInstance())
            {
                deviceManager->WaitForIdle();
            }
            assetsToDestroy.clear();
            LOG_DEBUG("[Asset Manager] Unused assets unloaded. Remaining: {}", m_LoadedAssets.size());
        }
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
        if (auto project = LockActiveProject())
        {
            const auto &absoluteFilepath = std::filesystem::absolute(project->GetProjectFilepath(filepath).string());

            std::unique_lock lock(m_AssetMutex);
            auto it = m_AssetHandleByPath.find(absoluteFilepath.generic_string());
            if (it != m_AssetHandleByPath.end())
            {
                return it->second;
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
        if (static_cast<uint64_t>(handle) == 0)
            return false;

        return m_AssetRegistry.contains(handle) || m_LoadedAssets.contains(handle);
    }

    Ref<Asset> AssetManager::Import(AssetHandle handle, const AssetMetaData &metadata, bool cacheResult)
    {
        IGN_PROFILE_FUNCTION();

        {
            std::unique_lock lock(m_AssetMutex);
            if (m_LoadedAssets.contains(handle))
            {
                return m_LoadedAssets[handle];
            }
        }

        static constexpr uint64_t MAX_CONCURRENT_LOAD_BYTES = 64 * 1024 * 1024;

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
        case AssetType::Terrain:
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
            if (cacheResult)
            {
                AssignAsset(handle, asset);
            }
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
            if (cacheResult)
            {
                AssignAsset(handle, asset);
            }
            break;
        }
        }

        LOG_ASSERT(isValidType, "[Asset Manager] Failed to import asset, please check the AssetType: {}", getterMetadata.filepath.generic_string());
        return asset;
    }
}
