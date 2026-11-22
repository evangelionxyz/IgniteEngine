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
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/scripting/lua/lua_script.hpp"
#include "ignite/terrain/terrain.hpp"
#include "ignite/asset/asset_worker.hpp"

#include "ignite/asset/asset.hpp"
#include "ignite/asset/serial.hpp"
#include "ignite/core/core.hpp"
#include <fbxsdk.h>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ignite
{
    static AssetMetaData s_NullMetaData;
    static AssetManager *s_AssetManagerInstance = nullptr;

    namespace
    {
        std::string NormalizeWatchKey(const std::filesystem::path &path)
        {
            std::error_code ec;
            std::filesystem::path absPath = std::filesystem::absolute(path, ec);
            if (ec)
                absPath = path;

            std::string key = absPath.lexically_normal().generic_string();
            while (key.size() > 1 && key.back() == '/')
                key.pop_back();

#ifdef PLATFORM_WINDOWS
            std::transform(key.begin(), key.end(), key.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
            return key;
        }

        std::string ToLowerAscii(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool ShouldIgnoreWatchedPath(const std::filesystem::path &path)
        {
            const std::string ext = ToLowerAscii(path.extension().string());
            if (ext == ".tmp" || ext == ".ixproj" || ext == ".ixreg" || ext == ".slnx" || ext == ".csproj" || ext == ".props")
                return true;

            for (const auto &part : path)
            {
                const std::string name = ToLowerAscii(part.string());
                if (name == ".cache" || name == ".vs" || name == ".git" || name == "bin" || name == "obj")
                    return true;
            }

            return false;
        }
    }

    static shared_task<Ref<Asset>> MakeCompletedAssetTask(Ref<Asset> asset)
    {
        co_return asset;
    }

    void AssetManager::VerifyNotRenderThread()
    {
        if (!Application::IsRenderThreadRunning())
        {
            return;
        }

        auto *app = Application::GetInstance();
        const std::thread *renderThread = app ? app->GetRenderThread() : nullptr;
        if (renderThread && std::this_thread::get_id() == renderThread->get_id())
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
            m_LoadScope.Wait();
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

        StopProjectAssetWatchers();

        if (auto *dm = DeviceManager::GetInstance())
        {
            if (auto *device = dm->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }
        }

        {
            std::lock_guard<std::mutex> relLock(m_DeferredReleaseMutex);
            m_DeferredReleases.clear();
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

        for (auto &[h, rec] : oldAssetRecords)
        {
            if (rec)
            {
                rec->loadTask = {};
                rec->asset.reset();
            }
        }
        oldAssetRecords.clear();

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
        {
            std::lock_guard<std::mutex> projectLock(m_ProjectMutex);
            m_Project = project;
        }

        if (project)
        {
            StartProjectAssetWatchers(project);
        }
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
            ignite_asset_assign_metadata(static_cast<uint64_t>(handle),
                metadata.filepath.generic_string().c_str(), metadata.type);
        }

        SyncFromRust();

        if (metadata.type == AssetType::StaticMesh || metadata.type == AssetType::SkeletalMesh)
        {
            TrackMeshSourceAsset(handle);
        }
    }

    const std::string AssetManager::GetAssetDisplayName(AssetHandle handle) const
    {
        EMeshType defaultMeshType = EMeshType::CUBE;
        if (Renderer::IsDefaultMeshHandle(handle, &defaultMeshType))
            return Renderer::GetDefaultMeshName(defaultMeshType);

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

        UntrackMeshSourceAsset(handle);

        {
            std::unique_lock lock(m_AssetMutex);

            // Sync removal with Rust AssetManager backend
            ignite_asset_remove_metadata(static_cast<uint64_t>(handle));
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

    shared_task<Ref<Asset>> AssetManager::RequestAssetAsync(AssetHandle handle)
    {
        std::lock_guard<std::mutex> scopeLock(m_LoadScopeMutex);
        if (m_ShuttingDown.load(std::memory_order_acquire) || handle == AssetHandle(0))
        {
            return MakeCompletedAssetTask(nullptr);
        }

        shared_task<Ref<Asset>> task;
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

        m_LoadScope.Spawn(task);
        return task;
    }

    Ref<Asset> AssetManager::TryGetAsset(AssetHandle handle) const
    {
        if (Renderer::IsDefaultMeshHandle(handle))
        {
            return Renderer::GetDefaultMeshByHandle(handle);
        }

        std::unique_lock lock(m_AssetMutex);
        const auto it = m_LoadedAssets.find(handle);
        return it != m_LoadedAssets.end() ? it->second : nullptr;
    }

    AssetState AssetManager::GetAssetState(AssetHandle handle) const
    {
        if (Renderer::IsDefaultMeshHandle(handle))
        {
            return AssetState::Ready;
        }

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

    shared_task<Ref<Asset>> AssetManager::LoadAssetPipeline(AssetHandle handle, AssetMetaData metadata,
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
            record.reset();
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
        const uint64_t rustVersion = ignite_asset_get_registry_version();
        if (rustVersion == m_LastSyncedRustVersion)
            return;

        const size_t count = ignite_asset_get_registry_count();
        if (count == 0)
        {
            m_LastSyncedRustVersion = rustVersion;
            return;
        }

        std::vector<IgniteAssetRegistryEntryFFI> entries(count);
        const size_t fetched = ignite_asset_get_registry_snapshot(entries.data(), count);

        std::vector<AssetHandle> newlyAddedMeshes;
        {
            std::unique_lock lock(m_AssetMutex);
            auto activeProject = LockActiveProject();
            for (size_t i = 0; i < fetched; ++i)
            {
                const AssetHandle handle = AssetHandle(entries[i].handle);
                if (handle == AssetHandle(0))
                    continue;

                const bool isNewHandle = !m_AssetRegistry.contains(handle);

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
                    m_AssetHandleByPath[NormalizeWatchKey(absoluteMetadataPath)] = handle;
                }

                if (isNewHandle && (metadata.type == AssetType::StaticMesh || metadata.type == AssetType::SkeletalMesh))
                {
                    newlyAddedMeshes.push_back(handle);
                }
            }

            m_LastSyncedRustVersion = rustVersion;
        }

        for (AssetHandle meshHandle : newlyAddedMeshes)
        {
            TrackMeshSourceAsset(meshHandle);
        }
    }

    void AssetManager::OnUpdate(float deltaTime)
    {
        (void)deltaTime;

        // Sync metadata registry from Rust source of truth
        SyncFromRust();

        // Drain deferred GPU-safe asset releases
        DrainDeferredReleases();

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
        auto activeProject = LockActiveProject();
        if (!activeProject)
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
                        std::vector<Ref<Material>> materialsToCheck;
                        {
                            std::unique_lock lock(m_AssetMutex);
                            for (const auto &[handle, asset] : m_LoadedAssets)
                            {
                                if (asset && asset->GetAssetType() == AssetType::Material)
                                {
                                    materialsToCheck.push_back(std::static_pointer_cast<Material>(asset));
                                }
                            }
                        }

                        for (const auto &material : materialsToCheck)
                        {
                            if (!material)
                                continue;

                            if (signal.type == AssetType::Material)
                            {
                                if (material->handle == signal.handle)
                                {
                                    material->InvalidateBindingSet();
                                }
                            }
                            else
                            {
                                auto isTextureBeingUsed = [](AssetHandle textureHandle, const Ref<Material> &mat) -> bool
                                    {
                                        return textureHandle == mat->baseColorTextureHandle
                                            || textureHandle == mat->emissiveTextureHandle
                                            || textureHandle == mat->metallicTextureHandle
                                            || textureHandle == mat->roughnessTextureHandle
                                            || textureHandle == mat->normalTextureHandle
                                            || textureHandle == mat->occlusionTextureHandle;
                                    };

                                const bool validTextureRequest = (signal.type == AssetType::Texture) && isTextureBeingUsed(signal.handle, material);
                                if (signal.type == AssetType::Environment || validTextureRequest)
                                {
                                    material->InvalidateBindingSet();
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
                    auto proj = LockActiveProject();
                    auto activeScene = proj ? proj->LockActiveScene() : nullptr;
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
            if (auto *dm = DeviceManager::GetInstance())
            {
                if (auto *device = dm->GetDevice())
                {
                    GPUUploadSync::DeviceWaitIdle(device);
                }
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
        if (Renderer::IsDefaultMeshHandle(handle))
            return AssetType::StaticMesh;

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
            const std::filesystem::path projectPath = project->GetProjectFilepath(filepath);
            std::error_code ec;
            const std::filesystem::path absoluteFilepath = std::filesystem::absolute(projectPath, ec);
            const std::filesystem::path lookupPath = ec ? projectPath : absoluteFilepath;

            std::unique_lock lock(m_AssetMutex);
            auto it = m_AssetHandleByPath.find(lookupPath.generic_string());
            if (it != m_AssetHandleByPath.end())
            {
                return it->second;
            }

            it = m_AssetHandleByPath.find(NormalizeWatchKey(lookupPath));
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

        if (Renderer::IsDefaultMeshHandle(handle))
            return true;

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
        case AssetType::LuaScript:
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

    // -------------------------------------------------------------------------
    // Asset FileWatcher, Hot Reload & Mesh Source Tracking
    // -------------------------------------------------------------------------

    void AssetManager::DeferReleaseResource(std::shared_ptr<void> resource, uint32_t framesToWait)
    {
        if (!resource)
            return;

        std::lock_guard<std::mutex> lock(m_DeferredReleaseMutex);
        m_DeferredReleases.emplace_back(std::max(1u, framesToWait), std::move(resource));
    }

    void AssetManager::DrainDeferredReleases()
    {
        std::vector<std::shared_ptr<void>> readyToDrop;
        {
            std::lock_guard<std::mutex> lock(m_DeferredReleaseMutex);
            if (m_DeferredReleases.empty())
                return;

            for (auto it = m_DeferredReleases.begin(); it != m_DeferredReleases.end();)
            {
                if (it->first <= 1)
                {
                    readyToDrop.push_back(std::move(it->second));
                    it = m_DeferredReleases.erase(it);
                }
                else
                {
                    --it->first;
                    ++it;
                }
            }
        }
        readyToDrop.clear();
    }

    void AssetManager::SuppressWatchPath(const std::filesystem::path &filepath, uint32_t durationMs)
    {
        if (filepath.empty())
            return;

        const std::string key = NormalizeWatchKey(filepath);
        const auto expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);

        std::lock_guard<std::mutex> lock(m_WatchMutex);
        m_SuppressedWatchPaths[key] = expiry;
    }

    bool AssetManager::IsWatchPathSuppressed(const std::filesystem::path &filepath)
    {
        const std::string key = NormalizeWatchKey(filepath);
        const auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(m_WatchMutex);
        auto it = m_SuppressedWatchPaths.find(key);
        if (it == m_SuppressedWatchPaths.end())
            return false;

        if (now <= it->second)
            return true;

        m_SuppressedWatchPaths.erase(it);
        return false;
    }

    void AssetManager::StartProjectAssetWatchers(const Ref<Project> &project)
    {
        if (!project)
            return;

        StopProjectAssetWatchers();

        auto watchDirectoryIfValid = [this](const std::filesystem::path &dir)
        {
            std::error_code ec;
            if (dir.empty() || !std::filesystem::is_directory(dir, ec) || ec)
                return;

            const FileWatchToken token = FileWatcher::WatchDirectory(dir,
                [this](const FileChangeEvent &event)
                {
                    OnAssetFileChanged(event);
                });

            if (token != kInvalidFileWatchToken)
            {
                std::lock_guard<std::mutex> lock(m_WatchMutex);
                m_DirectoryWatchTokens.push_back(token);
                LOG_INFO("[Asset Manager] Watching asset directory: {}", dir.generic_string());
            }
        };

        const std::filesystem::path assetDir = project->GetAssetDirectory();
        watchDirectoryIfValid(assetDir);

        const std::filesystem::path scriptsDir = project->GetScriptsDirectory();
        if (!scriptsDir.empty() && NormalizeWatchKey(scriptsDir) != NormalizeWatchKey(assetDir))
        {
            watchDirectoryIfValid(scriptsDir);
        }

        // Track source files for any meshes already present in the registry
        std::vector<AssetHandle> meshHandles;
        {
            std::unique_lock lock(m_AssetMutex);
            for (const auto &[handle, meta] : m_AssetRegistry)
            {
                if (meta.type == AssetType::StaticMesh || meta.type == AssetType::SkeletalMesh)
                {
                    meshHandles.push_back(handle);
                }
            }
        }

        for (AssetHandle handle : meshHandles)
        {
            TrackMeshSourceAsset(handle);
        }
    }

    void AssetManager::StopProjectAssetWatchers()
    {
        std::vector<FileWatchToken> tokensToUnwatch;
        {
            std::lock_guard<std::mutex> lock(m_WatchMutex);
            tokensToUnwatch.swap(m_DirectoryWatchTokens);
            for (const auto &[handle, watch] : m_MeshSourceWatches)
            {
                if (watch.sourceToken != kInvalidFileWatchToken)
                    tokensToUnwatch.push_back(watch.sourceToken);
                if (watch.companionBinToken != kInvalidFileWatchToken)
                    tokensToUnwatch.push_back(watch.companionBinToken);
            }
            m_MeshSourceWatches.clear();
            m_SuppressedWatchPaths.clear();
            m_ReloadsInFlight.clear();
            m_MeshSourceReimportsInFlight.clear();
        }

        for (FileWatchToken token : tokensToUnwatch)
        {
            FileWatcher::Unwatch(token);
        }
    }

    void AssetManager::TrackMeshSourceAsset(AssetHandle handle)
    {
        if (handle == AssetHandle(0))
            return;

        Ref<Project> project = LockActiveProject();
        if (!project)
            return;

        AssetMetaData metadata;
        {
            std::unique_lock lock(m_AssetMutex);
            auto it = m_AssetRegistry.find(handle);
            if (it == m_AssetRegistry.end())
                return;
            metadata = it->second;
        }

        if (metadata.type != AssetType::StaticMesh && metadata.type != AssetType::SkeletalMesh)
            return;

        const std::filesystem::path meshFullPath = project->GetProjectFilepath(metadata.filepath);
        const std::string projRootKey = NormalizeWatchKey(project->GetDirectory());
        const std::string meshPathKey = NormalizeWatchKey(meshFullPath);
        if (meshPathKey.size() <= projRootKey.size()
            || meshPathKey.compare(0, projRootKey.size(), projRootKey) != 0)
        {
            return;
        }
        std::filesystem::path rawSourcePath;

        if (Ref<Asset> loaded = TryGetAsset(handle))
        {
            rawSourcePath = loaded->GetSourcePath();
        }
        if (rawSourcePath.empty())
        {
            rawSourcePath = Asset::ReadSourcePathFromMetaFile(meshFullPath);
        }

        if (rawSourcePath.empty())
        {
            UntrackMeshSourceAsset(handle);
            return;
        }

        std::filesystem::path resolvedSource = rawSourcePath.is_absolute()
            ? rawSourcePath.lexically_normal()
            : project->GetProjectFilepath(rawSourcePath).lexically_normal();

        std::error_code ec;
        if (!std::filesystem::exists(resolvedSource, ec) || ec || !std::filesystem::is_regular_file(resolvedSource, ec))
        {
            // Only track mesh source assets if the source file exists on disk.
            UntrackMeshSourceAsset(handle);
            return;
        }

        const std::string resolvedKey = NormalizeWatchKey(resolvedSource);
        {
            std::lock_guard<std::mutex> lock(m_WatchMutex);
            auto existingIt = m_MeshSourceWatches.find(handle);
            if (existingIt != m_MeshSourceWatches.end()
                && existingIt->second.sourceToken != kInvalidFileWatchToken
                && NormalizeWatchKey(existingIt->second.sourcePath) == resolvedKey)
            {
                return;
            }
        }

        // Replace any previous watch for this handle
        UntrackMeshSourceAsset(handle);

        const FileWatchToken sourceToken = FileWatcher::WatchFile(resolvedSource,
            [this, handle, resolvedSource](const FileChangeEvent &ev)
            {
                OnMeshSourceFileChanged(handle, resolvedSource, ev);
            });

        if (sourceToken == kInvalidFileWatchToken)
            return;

        MeshSourceWatch watch{};
        watch.sourceToken = sourceToken;
        watch.sourcePath = resolvedSource;

        // If this is a .gltf file with an existing sibling .bin buffer, watch it as well.
        if (ToLowerAscii(resolvedSource.extension().string()) == ".gltf")
        {
            std::filesystem::path candidateBin = resolvedSource;
            candidateBin.replace_extension(".bin");
            if (std::filesystem::exists(candidateBin, ec) && !ec && std::filesystem::is_regular_file(candidateBin, ec))
            {
                watch.companionBinPath = candidateBin;
                watch.companionBinToken = FileWatcher::WatchFile(candidateBin,
                    [this, handle, resolvedSource](const FileChangeEvent &ev)
                    {
                        OnMeshSourceFileChanged(handle, resolvedSource, ev);
                    });
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_WatchMutex);
            m_MeshSourceWatches[handle] = std::move(watch);
        }

        LOG_INFO("[Asset Manager] Tracking mesh source '{}' for handle {}",
            resolvedSource.generic_string(), static_cast<uint64_t>(handle));
    }

    void AssetManager::UntrackMeshSourceAsset(AssetHandle handle)
    {
        FileWatchToken sourceToken = kInvalidFileWatchToken;
        FileWatchToken binToken = kInvalidFileWatchToken;

        {
            std::lock_guard<std::mutex> lock(m_WatchMutex);
            auto it = m_MeshSourceWatches.find(handle);
            if (it == m_MeshSourceWatches.end())
                return;

            sourceToken = it->second.sourceToken;
            binToken = it->second.companionBinToken;
            m_MeshSourceWatches.erase(it);
        }

        if (sourceToken != kInvalidFileWatchToken)
            FileWatcher::Unwatch(sourceToken);
        if (binToken != kInvalidFileWatchToken)
            FileWatcher::Unwatch(binToken);
    }

    bool AssetManager::IsMeshSourceWatched(AssetHandle handle) const
    {
        std::lock_guard<std::mutex> lock(m_WatchMutex);
        auto it = m_MeshSourceWatches.find(handle);
        return it != m_MeshSourceWatches.end() && it->second.sourceToken != kInvalidFileWatchToken;
    }

    std::filesystem::path AssetManager::GetWatchedMeshSourcePath(AssetHandle handle) const
    {
        std::lock_guard<std::mutex> lock(m_WatchMutex);
        auto it = m_MeshSourceWatches.find(handle);
        return it != m_MeshSourceWatches.end() ? it->second.sourcePath : std::filesystem::path{};
    }

    void AssetManager::OnAssetFileChanged(const FileChangeEvent &event)
    {
        if (m_ShuttingDown.load(std::memory_order_acquire) || event.path.empty())
            return;

        Ref<Project> project = LockActiveProject();
        if (!project)
            return;

        const std::filesystem::path projectRelPath = project->GetProjectRelativeFilepath(event.path);
        if (ShouldIgnoreWatchedPath(projectRelPath))
            return;

        if (IsWatchPathSuppressed(event.path))
            return;

        const std::string ext = ToLowerAscii(event.path.extension().string());

        // Handle sidecar .meta JSON changes (e.g. texture import settings or mesh source path updates)
        if (ext == ".meta")
        {
            const std::filesystem::path targetPath = event.path.parent_path() / event.path.stem();
            const AssetHandle targetHandle = GetAssetHandle(targetPath);
            if (targetHandle != AssetHandle(0))
            {
                const AssetType targetType = GetAssetType(targetHandle);
                if (targetType == AssetType::StaticMesh || targetType == AssetType::SkeletalMesh)
                {
                    TrackMeshSourceAsset(targetHandle);
                }
                else if (targetType == AssetType::Texture)
                {
                    ReloadAssetAsync(targetHandle);
                }
            }
            return;
        }

        AssetHandle handle = GetAssetHandle(event.path);

        if (event.type == FileChangeType::Removed)
        {
            if (handle != AssetHandle(0))
            {
                const AssetType type = GetAssetType(handle);
                UntrackMeshSourceAsset(handle);
                SignalBus::Emit(AssetChangeSignal{ handle, type });
            }
            return;
        }

        // If a brand new supported asset file is added inside Assets/, register its metadata.
        // Skip raw mesh source files (.fbx/.gltf/.glb) which are imported explicitly.
        if (handle == AssetHandle(0))
        {
            const AssetType detectedType = GetAssetTypeFromExtension(ext);
            if (detectedType != AssetType::Invalid
                && detectedType != AssetType::Mesh
                && detectedType != AssetType::Metadata
                && detectedType != AssetType::Project)
            {
                AssetMetaData newMeta;
                newMeta.filepath = project->GetProjectRelativeFilepath(event.path);
                newMeta.type = detectedType;
                handle = AssetHandle();
                AssignMetaData(handle, newMeta);
                LOG_INFO("[Asset Manager] Registered newly added asset: {}", newMeta.filepath.generic_string());
                SignalBus::Emit(AssetChangeSignal{ handle, detectedType });
            }
            return;
        }

        const AssetType type = GetAssetType(handle);
        if (type == AssetType::Invalid || type == AssetType::Scene || type == AssetType::Project)
            return;

        if (type == AssetType::StaticMesh || type == AssetType::SkeletalMesh)
        {
            TrackMeshSourceAsset(handle);
        }

        ReloadAssetAsync(handle);
    }

    void AssetManager::OnMeshSourceFileChanged(AssetHandle meshHandle, const std::filesystem::path &sourcePath, const FileChangeEvent &event)
    {
        if (m_ShuttingDown.load(std::memory_order_acquire) || meshHandle == AssetHandle(0))
            return;

        std::error_code ec;
        if (event.type == FileChangeType::Removed || !std::filesystem::exists(sourcePath, ec) || ec)
        {
            LOG_INFO("[Asset Manager] Mesh source asset removed/missing '{}'; stopping watch for handle {}",
                sourcePath.generic_string(), static_cast<uint64_t>(meshHandle));
            UntrackMeshSourceAsset(meshHandle);
            return;
        }

        Ref<Project> project = LockActiveProject();
        if (!project)
            return;

        AssetMetaData meshMeta;
        {
            std::unique_lock lock(m_AssetMutex);
            auto it = m_AssetRegistry.find(meshHandle);
            if (it == m_AssetRegistry.end())
            {
                UntrackMeshSourceAsset(meshHandle);
                return;
            }
            meshMeta = it->second;
        }

        if (meshMeta.type != AssetType::StaticMesh && meshMeta.type != AssetType::SkeletalMesh)
            return;

        {
            std::lock_guard<std::mutex> lock(m_WatchMutex);
            if (!m_MeshSourceReimportsInFlight.insert(meshHandle).second)
                return;
        }

        const std::filesystem::path existingMeshFullPath = project->GetProjectFilepath(meshMeta.filepath);
        const std::filesystem::path targetDir = existingMeshFullPath.parent_path();
        const AssetType meshType = meshMeta.type;
        const uint64_t managerGen = m_Generation.load(std::memory_order_acquire);

        LOG_INFO("[Asset Manager] Source asset changed '{}'; reloading mesh, materials, and thumbnails (handle {})",
            sourcePath.generic_string(), static_cast<uint64_t>(meshHandle));

        AssetWorker::SubmitJob("Reloading mesh from source...", [this, meshHandle, meshType, sourcePath, existingMeshFullPath, targetDir, managerGen]()
        {
            auto clearInFlight = [this, meshHandle]()
            {
                std::lock_guard<std::mutex> lock(m_WatchMutex);
                m_MeshSourceReimportsInFlight.erase(meshHandle);
            };

            if (m_ShuttingDown.load(std::memory_order_acquire) || m_Generation.load(std::memory_order_acquire) != managerGen)
            {
                clearInFlight();
                return;
            }

            std::vector<AssetHandle> updatedMaterials;
            std::vector<AssetHandle> updatedTextures;

            AssetMetaData sourceMetadata;
            sourceMetadata.filepath = sourcePath;
            sourceMetadata.type = meshType;

            bool success = false;
            if (meshType == AssetType::StaticMesh)
            {
                StaticMeshImportPayload payload{};
                payload.targetDirectory = targetDir;
                payload.outputMeshPath = existingMeshFullPath;
                payload.importMaterials = true;
                payload.forceRebuild = true;
                payload.overwriteExisting = true;
                payload.outUpdatedMaterials = &updatedMaterials;
                payload.outUpdatedTextures = &updatedTextures;

                Ref<StaticMesh> reloaded = AssetImporter::ImportStaticMesh(meshHandle, sourceMetadata, this, payload);
                success = (reloaded != nullptr);
            }
            else if (meshType == AssetType::SkeletalMesh)
            {
                SkeletalMeshImportPayload payload{};
                payload.targetDirectory = targetDir;
                payload.outputMeshPath = existingMeshFullPath;
                payload.importMesh = true;
                payload.importMaterials = true;
                payload.importSkeleton = true;
                payload.importAnimations = false;
                payload.forceRebuild = true;
                payload.overwriteExisting = true;
                payload.outUpdatedMaterials = &updatedMaterials;
                payload.outUpdatedTextures = &updatedTextures;

                Ref<SkeletalMesh> reloaded = AssetImporter::ImportSkeletalMesh(meshHandle, sourceMetadata, this, payload);
                success = (reloaded != nullptr);
            }

            clearInFlight();

            if (!success || m_ShuttingDown.load(std::memory_order_acquire) || m_Generation.load(std::memory_order_acquire) != managerGen)
                return;

            // Emit AssetChangeSignal on main thread for textures, materials, and mesh so
            // live scenes, material binding sets, and ContentBrowser/ThumbnailGenerator all refresh.
            Application::SubmitToMainThread([meshHandle, meshType, updatedTextures = std::move(updatedTextures), updatedMaterials = std::move(updatedMaterials)]()
            {
                for (AssetHandle texHandle : updatedTextures)
                {
                    if (texHandle != AssetHandle(0))
                        SignalBus::Emit(AssetChangeSignal{ texHandle, AssetType::Texture });
                }
                for (AssetHandle matHandle : updatedMaterials)
                {
                    if (matHandle != AssetHandle(0))
                        SignalBus::Emit(AssetChangeSignal{ matHandle, AssetType::Material });
                }
                SignalBus::Emit(AssetChangeSignal{ meshHandle, meshType });
            }, "AssetManager::OnMeshSourceReimported");
        });
    }

    void AssetManager::ReloadAssetAsync(AssetHandle handle)
    {
        if (m_ShuttingDown.load(std::memory_order_acquire) || handle == AssetHandle(0))
            return;

        {
            std::lock_guard<std::mutex> lock(m_WatchMutex);
            if (!m_ReloadsInFlight.insert(handle).second)
                return;
        }

        const uint64_t managerGen = m_Generation.load(std::memory_order_acquire);

        AssetWorker::SubmitJob([this, handle, managerGen]()
        {
            struct InFlightGuard
            {
                AssetManager *mgr;
                AssetHandle h;
                ~InFlightGuard()
                {
                    std::lock_guard<std::mutex> lock(mgr->m_WatchMutex);
                    mgr->m_ReloadsInFlight.erase(h);
                }
            } guard{ this, handle };

            if (m_ShuttingDown.load(std::memory_order_acquire) || m_Generation.load(std::memory_order_acquire) != managerGen)
                return;

            ReloadAssetImmediate(handle);
        });
    }

    bool AssetManager::ReloadAssetImmediate(AssetHandle handle)
    {
        if (m_ShuttingDown.load(std::memory_order_acquire) || handle == AssetHandle(0))
            return false;

        Ref<Project> project = LockActiveProject();
        if (!project)
            return false;

        AssetMetaData metadata;
        Ref<Asset> existingAsset;
        {
            std::unique_lock lock(m_AssetMutex);
            auto metaIt = m_AssetRegistry.find(handle);
            if (metaIt == m_AssetRegistry.end())
                return false;
            metadata = metaIt->second;

            auto loadedIt = m_LoadedAssets.find(handle);
            if (loadedIt != m_LoadedAssets.end())
                existingAsset = loadedIt->second;
        }

        const std::filesystem::path fullPath = project->GetProjectFilepath(metadata.filepath);
        std::error_code ec;
        if (fullPath.empty() || !std::filesystem::exists(fullPath, ec) || ec)
            return false;

        // Even if the asset is not currently loaded in memory, emit AssetChangeSignal so
        // cached thumbnails in ContentBrowser / ThumbnailGenerator are invalidated & refreshed.
        if (!existingAsset)
        {
            Application::SubmitToMainThread([handle, type = metadata.type]()
            {
                SignalBus::Emit(AssetChangeSignal{ handle, type });
            }, "AssetManager::NotifyUnloadedAssetChange");
            return true;
        }

        Ref<Asset> reloadedAsset;
        try
        {
            if (metadata.type == AssetType::Texture)
            {
                AssetMetaData textureMetadata = metadata;
                textureMetadata.filepath = fullPath;
                TextureCreateInfo createInfo = Texture::GetDefaultCreateInfo(metadata);
                if (!Texture::LoadCreateInfoFile(Texture::GetMetaPath(project.get(), metadata), createInfo))
                {
                    Texture::LoadCreateInfoFile(Texture::GetLegacyMetaPath(project.get(), metadata), createInfo);
                }
                reloadedAsset = AssetImporter::ImportTexture(handle, textureMetadata, createInfo, this);
            }
            else
            {
                reloadedAsset = AssetImporter::Import(handle, metadata, this);
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("[Asset Manager] Failed to reload asset '{}' (handle {}): {}",
                metadata.filepath.generic_string(), static_cast<uint64_t>(handle), e.what());
            return false;
        }
        catch (...)
        {
            LOG_ERROR("[Asset Manager] Failed to reload asset '{}' (handle {}): unknown error",
                metadata.filepath.generic_string(), static_cast<uint64_t>(handle));
            return false;
        }

        if (!reloadedAsset)
            return false;

        return ApplyReloadedAsset(handle, metadata, reloadedAsset);
    }

    bool AssetManager::ApplyReloadedAsset(AssetHandle handle, const AssetMetaData &metadata, const Ref<Asset> &reloadedAsset)
    {
        if (!reloadedAsset)
            return false;

        reloadedAsset->handle = handle;
        Ref<Asset> existingAsset = TryGetAsset(handle);

        switch (metadata.type)
        {
            case AssetType::Material:
            {
                if (existingAsset)
                {
                    Ref<Material> dst = existingAsset->As<Material>();
                    Ref<Material> src = reloadedAsset->As<Material>();
                    if (dst && src)
                    {
                        Application::SubmitToRenderThread([dst, src]()
                        {
                            dst->name = src->name;
                            dst->SetType(src->GetType());
                            dst->SetCullMode(src->GetCullMode());
                            dst->baseColorTextureHandle = src->baseColorTextureHandle;
                            dst->emissiveTextureHandle = src->emissiveTextureHandle;
                            dst->metallicTextureHandle = src->metallicTextureHandle;
                            dst->roughnessTextureHandle = src->roughnessTextureHandle;
                            dst->normalTextureHandle = src->normalTextureHandle;
                            dst->occlusionTextureHandle = src->occlusionTextureHandle;
                            dst->gpuData = src->gpuData;
                            dst->InvalidateBindingSet();
                            dst->SetDirtyFlag(false);
                            dst->SetReadyFlag(true);
                        }, "AssetManager::ReloadMaterialInPlace");
                    }
                }
                else
                {
                    AssignAsset(handle, reloadedAsset);
                }
                break;
            }
            case AssetType::Material2D:
            {
                if (existingAsset)
                {
                    Ref<Material2D> dst = existingAsset->As<Material2D>();
                    Ref<Material2D> src = reloadedAsset->As<Material2D>();
                    if (dst && src)
                    {
                        Application::SubmitToMainThread([dst, src]()
                        {
                            dst->name = src->name;
                            dst->textureHandle = src->textureHandle;
                            dst->data = src->data;
                            dst->SetDirtyFlag(false);
                            dst->SetReadyFlag(true);
                        }, "AssetManager::ReloadMaterial2DInPlace");
                    }
                }
                else
                {
                    AssignAsset(handle, reloadedAsset);
                }
                break;
            }
            case AssetType::SpriteSheet:
            {
                if (existingAsset)
                {
                    Ref<SpriteSheet> dst = existingAsset->As<SpriteSheet>();
                    Ref<SpriteSheet> src = reloadedAsset->As<SpriteSheet>();
                    if (dst && src)
                    {
                        Application::SubmitToMainThread([dst, src]()
                        {
                            dst->SetAtlasSize(src->GetAtlasSize());
                            dst->SetTextureHandle(src->GetTextureHandle());
                            dst->GetSprites() = src->GetSprites();
                            dst->SetDirtyFlag(false);
                            dst->SetReadyFlag(true);
                        }, "AssetManager::ReloadSpriteSheetInPlace");
                    }
                }
                else
                {
                    AssignAsset(handle, reloadedAsset);
                }
                break;
            }
            case AssetType::StaticMesh:
            {
                if (existingAsset)
                {
                    Ref<StaticMesh> dst = existingAsset->As<StaticMesh>();
                    Ref<StaticMesh> src = reloadedAsset->As<StaticMesh>();
                    if (dst && src)
                    {
                        auto newInstances = src->GetMeshInstances();
                        const std::filesystem::path newSourcePath = src->GetSourcePath();
                        Application::SubmitToRenderThread([this, dst, newInstances = std::move(newInstances), newSourcePath]() mutable
                        {
                            auto oldInstances = std::make_shared<std::vector<Ref<StaticMeshInstance>>>(dst->GetMeshInstances());
                            dst->SetMeshInstances(newInstances);
                            dst->SetSourcePath(newSourcePath);
                            dst->CalculateLocalAABB();
                            dst->SetDirtyFlag(false);
                            dst->SetReadyFlag(true);
                            DeferReleaseResource(std::move(oldInstances));
                        }, "AssetManager::ReloadStaticMeshInPlace");
                    }
                }
                else
                {
                    AssignAsset(handle, reloadedAsset);
                }
                break;
            }
            case AssetType::SkeletalMesh:
            {
                if (existingAsset)
                {
                    Ref<SkeletalMesh> dst = existingAsset->As<SkeletalMesh>();
                    Ref<SkeletalMesh> src = reloadedAsset->As<SkeletalMesh>();
                    if (dst && src)
                    {
                        auto newInstances = src->GetMeshInstances();
                        const AssetHandle newSkel = src->GetSkeletonHandle();
                        const AssetHandle newAnim = src->GetAnimatorHandle();
                        const std::filesystem::path newSourcePath = src->GetSourcePath();
                        Application::SubmitToRenderThread([this, dst, newInstances = std::move(newInstances), newSkel, newAnim, newSourcePath]() mutable
                        {
                            auto oldInstances = std::make_shared<std::vector<Ref<SkeletalMeshInstance>>>(dst->GetMeshInstances());
                            dst->SetMeshInstances(newInstances);
                            dst->SetSkeleton(newSkel);
                            dst->SetAnimator(newAnim);
                            dst->SetSourcePath(newSourcePath);
                            dst->CalculateLocalAABB();
                            dst->SetDirtyFlag(false);
                            dst->SetReadyFlag(true);
                            DeferReleaseResource(std::move(oldInstances));
                        }, "AssetManager::ReloadSkeletalMeshInPlace");
                    }
                }
                else
                {
                    AssignAsset(handle, reloadedAsset);
                }
                break;
            }
            case AssetType::LuaScript:
            {
                if (existingAsset)
                {
                    Ref<lua::Script> dst = existingAsset->As<lua::Script>();
                    if (dst)
                    {
                        lua::Script::InvalidateFieldCache(dst->GetFilepath());
                        dst->Reload();
                    }
                }
                else
                {
                    AssignAsset(handle, reloadedAsset);
                }
                break;
            }
            default:
            {
                if (existingAsset)
                {
                    DeferReleaseResource(existingAsset);
                }
                AssignAsset(handle, reloadedAsset);
                break;
            }
        }

        LOG_INFO("[Asset Manager] Hot-reloaded {} '{}' (handle {})",
            AssetTypeToString(metadata.type), metadata.filepath.generic_string(), static_cast<uint64_t>(handle));

        Application::SubmitToMainThread([handle, type = metadata.type]()
        {
            SignalBus::Emit(AssetChangeSignal{ handle, type });
        }, "AssetManager::NotifyAssetReloaded");

        return true;
    }
}
