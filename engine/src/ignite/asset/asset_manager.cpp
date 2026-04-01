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
#include "ignite/serializer/serializer.hpp"

#include "ignite/core/logger.hpp"
#include <cstdint>

#include <fbxsdk.h>

namespace ignite {

	static std::mutex s_AssetThreadMutex;
	static AssetMetaData s_NullMetaData;

    namespace
    {
        static TextureCreateInfo GetDefaultTextureCreateInfo(const AssetMetaData &metadata)
        {
            TextureCreateInfo createInfo;
            const std::string extension = metadata.filepath.extension().string();
            const bool isHDR = extension == ".hdr";

            createInfo.format = isHDR ? nvrhi::Format::RGBA32_FLOAT : nvrhi::Format::RGBA8_UNORM;
            createInfo.mipLevels = isHDR ? 1 : 4;
            createInfo.flip = isHDR;
            createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
            createInfo.keepInitialState = true;
            createInfo.deferGpuCreate = true;

            return createInfo;
        }

        static std::filesystem::path GetTextureInfoPath(Project *project, const AssetMetaData &metadata)
        {
            if (!project)
            {
                return {};
            }

            std::filesystem::path texturePath = project->GetAssetFilepath(metadata.filepath);
            texturePath += ".ixtex";
            return texturePath;
        }

        static void SaveTextureCreateInfoFile(const std::filesystem::path &filepath, const TextureCreateInfo &createInfo)
        {
            if (filepath.empty())
            {
                return;
            }

            Serializer sr(filepath);
            sr.BeginMap();
            sr.BeginMap("TextureImportSettings");

            sr.AddKeyValue("Width", createInfo.width);
            sr.AddKeyValue("Height", createInfo.height);
            sr.AddKeyValue("Depth", createInfo.depth);
            sr.AddKeyValue("MipLevels", createInfo.mipLevels);
            sr.AddKeyValue("ArraySize", createInfo.arraySize);
            sr.AddKeyValue("SampleCount", createInfo.sampleCount);
            sr.AddKeyValue("SampleQuality", createInfo.sampleQuality);

            sr.AddKeyValue("Flip", createInfo.flip);
            sr.AddKeyValue("IsRenderTarget", createInfo.isRenderTarget);
            sr.AddKeyValue("IsTypeless", createInfo.isTypeless);
            sr.AddKeyValue("IsUAV", createInfo.isUAV);
            sr.AddKeyValue("IsShadingRateSurface", createInfo.isShadingRateSurface);
            sr.AddKeyValue("KeepCpuData", createInfo.keepCpuData);
            sr.AddKeyValue("DeferGpuCreate", createInfo.deferGpuCreate);
            sr.AddKeyValue("KeepInitialState", createInfo.keepInitialState);
            sr.AddKeyValue("SamplerLinearFiltering", createInfo.samplerLinearFiltering);

            sr.AddKeyValue("Format", static_cast<uint32_t>(createInfo.format));
            sr.AddKeyValue("InitialState", static_cast<uint32_t>(createInfo.initialState));
            sr.AddKeyValue("Dimension", static_cast<uint32_t>(createInfo.dimension));
            sr.AddKeyValue("SamplerAddressU", static_cast<uint32_t>(createInfo.samplerAddressU));
            sr.AddKeyValue("SamplerAddressV", static_cast<uint32_t>(createInfo.samplerAddressV));
            sr.AddKeyValue("SamplerAddressW", static_cast<uint32_t>(createInfo.samplerAddressW));

            sr.EndMap();
            sr.EndMap();
            sr.Serialize();
        }

        static bool LoadTextureCreateInfoFile(const std::filesystem::path &filepath, TextureCreateInfo &outCreateInfo)
        {
            if (filepath.empty() || !std::filesystem::exists(filepath))
            {
                return false;
            }

            YAML::Node root = Serializer::Deserialize(filepath);
            YAML::Node node = root["TextureImportSettings"];
            if (!node)
            {
                return false;
            }

            if (node["Width"]) outCreateInfo.width = node["Width"].as<uint32_t>();
            if (node["Height"]) outCreateInfo.height = node["Height"].as<uint32_t>();
            if (node["Depth"]) outCreateInfo.depth = node["Depth"].as<uint32_t>();
            if (node["MipLevels"]) outCreateInfo.mipLevels = node["MipLevels"].as<uint32_t>();
            if (node["ArraySize"]) outCreateInfo.arraySize = node["ArraySize"].as<uint32_t>();
            if (node["SampleCount"]) outCreateInfo.sampleCount = node["SampleCount"].as<uint32_t>();
            if (node["SampleQuality"]) outCreateInfo.sampleQuality = node["SampleQuality"].as<uint32_t>();

            if (node["Flip"]) outCreateInfo.flip = node["Flip"].as<bool>();
            if (node["IsRenderTarget"]) outCreateInfo.isRenderTarget = node["IsRenderTarget"].as<bool>();
            if (node["IsTypeless"]) outCreateInfo.isTypeless = node["IsTypeless"].as<bool>();
            if (node["IsUAV"]) outCreateInfo.isUAV = node["IsUAV"].as<bool>();
            if (node["IsShadingRateSurface"]) outCreateInfo.isShadingRateSurface = node["IsShadingRateSurface"].as<bool>();
            if (node["KeepCpuData"]) outCreateInfo.keepCpuData = node["KeepCpuData"].as<bool>();
            if (node["DeferGpuCreate"]) outCreateInfo.deferGpuCreate = node["DeferGpuCreate"].as<bool>();
            if (node["KeepInitialState"]) outCreateInfo.keepInitialState = node["KeepInitialState"].as<bool>();
            if (node["SamplerLinearFiltering"]) outCreateInfo.samplerLinearFiltering = node["SamplerLinearFiltering"].as<bool>();

            if (node["Format"]) outCreateInfo.format = static_cast<nvrhi::Format>(node["Format"].as<uint32_t>());
            if (node["InitialState"]) outCreateInfo.initialState = static_cast<nvrhi::ResourceStates>(node["InitialState"].as<uint32_t>());
            if (node["Dimension"]) outCreateInfo.dimension = static_cast<nvrhi::TextureDimension>(node["Dimension"].as<uint32_t>());
            if (node["SamplerAddressU"]) outCreateInfo.samplerAddressU = static_cast<nvrhi::SamplerAddressMode>(node["SamplerAddressU"].as<uint32_t>());
            if (node["SamplerAddressV"]) outCreateInfo.samplerAddressV = static_cast<nvrhi::SamplerAddressMode>(node["SamplerAddressV"].as<uint32_t>());
            if (node["SamplerAddressW"]) outCreateInfo.samplerAddressW = static_cast<nvrhi::SamplerAddressMode>(node["SamplerAddressW"].as<uint32_t>());

            return true;
        }
    }

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

        if (metadata.type == AssetType::Texture)
        {
            TextureCreateInfo createInfo = GetDefaultTextureCreateInfo(metadata);
            const std::filesystem::path infoPath = GetTextureInfoPath(s_Project, metadata);
            if (!LoadTextureCreateInfoFile(infoPath, createInfo))
            {
                SaveTextureCreateInfoFile(infoPath, createInfo);
            }

            std::unique_lock lock(s_AssetThreadMutex);
            m_TextureCreateInfos[handle] = createInfo;
        }
        else
        {
            std::unique_lock lock(s_AssetThreadMutex);
            m_TextureCreateInfos.erase(handle);
        }
    }

    TextureCreateInfo AssetManager::GetTextureCreateInfo(AssetHandle handle) const
    {
        if (!IsAssetHandleValid(handle))
        {
            return TextureCreateInfo{};
        }

        {
            std::unique_lock lock(s_AssetThreadMutex);
            if (auto it = m_TextureCreateInfos.find(handle); it != m_TextureCreateInfos.end())
            {
                return it->second;
            }
        }

        const AssetMetaData &metadata = GetMetaData(handle);
        TextureCreateInfo createInfo = GetDefaultTextureCreateInfo(metadata);
        const std::filesystem::path infoPath = GetTextureInfoPath(s_Project, metadata);
        if (!LoadTextureCreateInfoFile(infoPath, createInfo))
        {
            SaveTextureCreateInfoFile(infoPath, createInfo);
        }

        return createInfo;
    }

    void AssetManager::SetTextureCreateInfo(AssetHandle handle, const TextureCreateInfo &createInfo, bool saveToDisk)
    {
        if (!IsAssetHandleValid(handle))
        {
            return;
        }

        const AssetMetaData &metadata = GetMetaData(handle);
        if (metadata.type != AssetType::Texture)
        {
            return;
        }

        {
            std::unique_lock lock(s_AssetThreadMutex);
            m_TextureCreateInfos[handle] = createInfo;
        }

        if (saveToDisk)
        {
            SaveTextureCreateInfoFile(GetTextureInfoPath(s_Project, metadata), createInfo);
        }
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
        case AssetType::SpriteSheet:
        case AssetType::Font:
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
            if (getterMetadata.type == AssetType::Texture)
            {
                AssetMetaData textureMetadata = getterMetadata;
                textureMetadata.filepath = AssetManager::GetProject()->GetAssetFilepath(getterMetadata.filepath);
                const TextureCreateInfo createInfo = GetTextureCreateInfo(handle);
                asset = AssetImporter::ImportTexture(handle, textureMetadata, createInfo);
            }
            else
            {
                asset = AssetImporter::Import(handle, getterMetadata);
            }
            
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
