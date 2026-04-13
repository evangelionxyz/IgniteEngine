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

#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include "asset.hpp"
#include "ignite/graphics/texture.hpp"

#include <map>
#include <unordered_set>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>

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

        Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata, AssetType requestedAssetType = AssetType::Auto);
        AssetHandle ImportAsset(const std::filesystem::path &filepath);

        void AssignMetaData(AssetHandle handle, const AssetMetaData &metadata);
        TextureCreateInfo GetTextureCreateInfo(AssetHandle handle) const;
        void SetTextureCreateInfo(AssetHandle handle, const TextureCreateInfo &createInfo, bool saveToDisk = true);

        const std::string GetAssetDisplayName(AssetHandle handle) const;

        template<typename T>
        void AssignAsset(AssetHandle handle, const Ref<T> &asset)
        {
            if (asset && std::is_base_of_v<Asset, T>)
            {
                m_LoadedAssets[handle] = asset;
                
                // Notify listeners that asset was loaded
                for (const auto &callback : m_LoadedCallbacks)
                {
                    callback(handle, asset->GetAssetType());
                }
            }
        }

        void RemoveAsset(AssetHandle handle);
        
        // Register callback to be notified when assets are loaded
        void RegisterAssetLoadedCallback(AssetLoadedCallback callback)
        {
            m_LoadedCallbacks.push_back(callback);
        }

        void SubmitJob(AssetJob job);

        Ref<Asset> GetAsset(AssetHandle handle, AssetType requestedAssetType = AssetType::Auto);
        Ref<Asset> GetAssetImmediate(AssetHandle handle, AssetType requestedAssetType = AssetType::Auto); // Synchronous load - blocks until complete
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
        std::unordered_map<AssetHandle, TextureCreateInfo> m_TextureCreateInfos;
        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
        std::unordered_set<AssetHandle> m_LoadingAssets; // Track assets currently being loaded
        std::vector<AssetLoadedCallback> m_LoadedCallbacks;

        std::condition_variable m_ConditionVariable;
        std::vector<std::thread> m_Workers;
        std::queue<AssetJob> m_Jobs;
        Project *m_Project;

        bool m_Running;

        std::mutex m_FbxSdkMutex;
        fbxsdk::FbxManager *m_FbxSdkManager = nullptr;
    };

}

#endif