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

#include "asset_importer.hpp"
#include "asset_static_mesh.hpp"

#include "ignite/audio/fmod_audio.hpp"
#include "ignite/audio/fmod_sound.hpp"

#include "ignite/core/application.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/scene/scene.hpp"

namespace ignite {

    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &)>> s_ImportFunctions =
    {
        { AssetType::Scene, AssetImporter::ImportScene },
        { AssetType::Texture, AssetImporter::ImportTexture },
        { AssetType::Audio, AssetImporter::ImportAudio },
        { AssetType::StaticMesh, AssetImporter::ImportStaticMesh }
    };

    Ref<Asset> AssetImporter::Import(AssetHandle handle, const AssetMetaData &metadata)
    {
        // should be always importing with full filepath
        AssetMetaData metadataCopy = metadata;
        metadataCopy.filepath = AssetManager::GetProject()->GetAssetFilepath(metadata.filepath);

        if (s_ImportFunctions.contains(metadataCopy.type))
        {
            return s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);
        }

        return nullptr;
    }

    void AssetImporter::ImportAsync(AssetHandle handle, const AssetMetaData &metadata, std::function<void(Ref<Asset>, AssetHandle)> callback)
    {
        AssetManager::GetProject()->GetAssetManager().SubmitJob([handle, metadata, callback]()
        {
            // should be always importing with full filepath
            AssetMetaData metadataCopy = metadata;
            metadataCopy.filepath = AssetManager::GetProject()->GetAssetFilepath(metadata.filepath);

            Ref<Asset> asset;
            if (s_ImportFunctions.contains(metadataCopy.type))
                asset = s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);
            
            if (asset)
            {
                callback(asset, handle);
            }
        });
    }

	Ref<AssetStaticMesh> AssetImporter::ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata)
	{
        // Generate folders
        auto meshScene = MeshLoader::LoadSceneGraphFromGLTF(metadata.filepath.generic_string());

        Ref<MeshInstance> mesh;

        return nullptr;
	}

	Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<Scene> scene = SceneSerializer::Deserialize(metadata.filepath, AssetManager::GetProject());
        if (scene)
        {
            scene->handle = handle;
        }
        return scene;
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<Texture> result = CreateRef<Texture>();
        Renderer::Submit([result, metadata, h = handle] (nvrhi::ICommandList *cmd) mutable
        {
            TextureCreateInfo createInfo;
            createInfo.format = nvrhi::Format::RGBA8_UNORM;
            createInfo.mipLevels = 4;
            createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
            createInfo.keepInitialState = true; // should keep initial state

            Ref<Texture> t = Texture::Create(metadata.filepath, createInfo, cmd);
            t->handle = h;

            *result = *t;
        });

        return result;
    }

    Ref<FmodSound> AssetImporter::ImportAudio(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<FmodSound> sound = FmodSound::Create(metadata.filepath.filename().string(), metadata.filepath.generic_string(), FMOD_DEFAULT);
        
        if (sound)
        {
            sound->handle = handle;
        }

        return sound;
    }
}
