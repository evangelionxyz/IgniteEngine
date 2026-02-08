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

#pragma once

#include "asset.hpp"

#include <map>
#include <vector>
#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>

namespace ignite {

    using AssetRegistry = std::map<AssetHandle, AssetMetaData>;
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

        template<typename T>
        void AssignAsset(AssetHandle handle, const Ref<T> &asset)
        {
            if (asset && std::is_base_of_v<Asset, T>)
            {
                m_LoadedAssets[handle] = asset;
            }
        }

        void RemoveAsset(AssetHandle handle);

        void SubmitJob(AssetJob job);

        Ref<Asset> GetAsset(AssetHandle handle);
        AssetType GetAssetType(AssetHandle handle) const;

        const AssetMetaData &GetMetaData(const std::filesystem::path &filepath, AssetHandle &outHandle);
        const AssetMetaData &GetMetaData(AssetHandle handle) const;
        
        AssetHandle GetAssetHandle(const std::filesystem::path &filepath);
        
        const std::filesystem::path &GetFilepath(AssetHandle handle) const;
        bool IsAssetHandleValid(AssetHandle handle) const;
    
        AssetRegistry &GetAssetAssetRegistry() { return m_AssetRegistry; }

        static Project *GetProject();

    private:
        void WorkerLoop();

        AssetRegistry m_AssetRegistry;
        std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;

        std::condition_variable m_ConditionVariable;
        std::vector<std::thread> m_Workers;
        std::mutex m_Mutex;
        std::queue<AssetJob> m_Jobs;
        bool m_Running;
    };

}
