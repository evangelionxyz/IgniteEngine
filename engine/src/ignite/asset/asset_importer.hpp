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
#include <future>
#include <nvrhi/nvrhi.h>

#include "ignite/scene/entity.hpp"

namespace ignite {

    struct FmodSound;
    class Environment;
    class GraphicsPipeline;
    class Scene;

    class AssetImporter
    {
    public:
        static void SyncMainThread();
        static Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<FmodSound> ImportAudio(AssetHandle handle, const AssetMetaData &metadata);
    };

    class MeshImporter : public AssetImporter
    {
    public:
        static Ref<Asset> ImportMeshSource(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Asset> ImportSkeletalMesh(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Asset> ImportSkeleton(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Asset> ImportAnimation(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Asset> ImportMaterial(AssetHandle handle, const AssetMetaData &metadata);
    };

    class EnvironmentImporter : public AssetImporter
    {
    public:
        static void Import(Ref<Environment> *outEnvironment, const std::string &filepath);
        static void UpdateTexture(Ref<Environment> *outEnvironment, const std::string &filepath);
        static void SyncMainThread();

    private:
        static Ref<Environment> ImportAsync(Ref<Environment> *outEnvironment, const std::string &filepath);
        static Ref<Environment> LoadTextureAsync(Ref<Environment> *outEnvironment, const std::string &filepath);
        static std::future<Ref<Environment>> m_Future;
    };
}
