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
    class AssetStaticMesh;
    class Scene;

	struct PendingFileLoading
	{
		enum Type : uint8_t
		{
			None = 0,
			Open,
			Save,
			ImportAssets,
		};

		Type type = None;
		AssetMetaData metadata;
		void *userData = nullptr;
	};

    class AssetImporter
    {
    public:
        static Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata);
        static void ImportAsync(AssetHandle handle, const AssetMetaData &metadata, std::function<void(Ref<Asset>, AssetHandle)> callback);

        static Ref<AssetStaticMesh> ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata);

        static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<FmodSound> ImportAudio(AssetHandle handle, const AssetMetaData &metadata);
    };
}
