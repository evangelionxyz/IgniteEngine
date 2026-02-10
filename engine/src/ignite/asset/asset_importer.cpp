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

#include "ignite/audio/fmod_audio.hpp"
#include "ignite/audio/fmod_sound.hpp"

#include "ignite/core/application.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/scene/scene.hpp"

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace ignite {

    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &)>> s_ImportFunctions =
    {
        { AssetType::Scene, AssetImporter::ImportScene },
        { AssetType::Texture, AssetImporter::ImportTexture },
        { AssetType::Audio, AssetImporter::ImportAudio },
        { AssetType::StaticMesh, AssetImporter::ImportStaticMesh },
        { AssetType::Material, AssetImporter::ImportMaterial }
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
            {
                asset = s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);
            }
            
            if (asset)
            {
                callback(asset, handle);
            }
        });
    }

	Ref<StaticMesh> AssetImporter::ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata)
	{
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

		static auto staticMeshBinExt = GetAssetExtensionFromType(AssetType::StaticMesh);
		static auto materialBinExt = GetAssetExtensionFromType(AssetType::Material);

		Ref<StaticMesh> asset;

		// Load the mesh from .ixsm
        if (metadata.filepath.extension() == staticMeshBinExt)
        {
		    asset = BinarySerializer::DeserializeStaticMesh(metadata.filepath);
        }

        if (asset)
        {
			for (auto &mesh : asset->GetMeshInstances())
			{
			    // Load materials
				AssetHandle materialHandle = mesh->GetMaterialHandle();
				AssetMetaData metadata = Project::GetInstance()->GetAssetManager().GetMetaData(materialHandle);
				if (metadata.type == AssetType::Material)
				{
					const auto &materialFilepath = Project::GetInstance()->GetAssetFilepath(metadata.filepath);
					Ref<Material> material = BinarySerializer::DeserializeMaterial(materialFilepath);
                    Project::GetInstance()->GetAssetManager().AssignAsset(materialHandle, material);

                    // Submit GPU upload command list to render thread (thread-safe)
                    Application::SubmitToMainThread([m = mesh, mat = material]()
                        {
                            nvrhi::IDevice* device = Application::GetGraphicsDevice();
                            nvrhi::CommandListHandle cmd = device->createCommandList();
                            cmd->open();
					        m->GetPrimitive()->CreateBuffer(cmd);
					        mat->SetTextureData(cmd);
                            cmd->close();
                            
                            // Submit to render thread's queue (thread-safe)
                            Application::SubmitWorkerCommandList(cmd);

                            return true;
                        });
				}
			}

            return asset;
        }

        const std::filesystem::path parentPath = metadata.filepath.parent_path();
        
        // Get project asset directory
        const std::filesystem::path projectAssetPath = Project::GetInstance()->GetAssetDirectory();
        
        const std::filesystem::path filename = metadata.filepath.stem();
        const std::filesystem::path outputDirectory = projectAssetPath / filename; // inside project asset directory
        const std::filesystem::path meshDirectory = outputDirectory / "StaticMesh";
        const std::filesystem::path materialDirectory = outputDirectory / "Material";
        const std::filesystem::path textureDirectory = outputDirectory / "Textures";

        // Create Output Directory
        if (!std::filesystem::exists(outputDirectory))
        {
            std::filesystem::create_directory(outputDirectory);
        }

        // Create Mesh Directory
        if (!std::filesystem::exists(meshDirectory))
        {
            std::filesystem::create_directory(meshDirectory);
        }

        // Create Material Directory
        if (!std::filesystem::exists(materialDirectory))
        {
            std::filesystem::create_directory(materialDirectory);
        }

        // Create Texture Directory
        if (!std::filesystem::exists(textureDirectory))
        {
			std::filesystem::create_directory(textureDirectory);
        }

		std::filesystem::path meshBinaryFilename = filename;
		meshBinaryFilename = meshBinaryFilename.replace_extension(staticMeshBinExt);
		std::filesystem::path meshBinaryFullpath = meshDirectory / meshBinaryFilename;

        if (!asset)
        {
			// Generate folders
			MeshScene meshScene;
			MeshLoader::LoadSceneGraphFromGLTF(metadata.filepath.generic_string(), meshScene);

            // Import and store material first
			for (Ref<Material> &mat : meshScene.materials)
			{
                const std::string materialFilename = mat->name+materialBinExt;
                std::filesystem::path materialBinFullPath = materialDirectory / materialFilename;
                BinarySerializer::SerializeMaterial(mat, materialBinFullPath);

                AssetHandle materialHandle = AssetHandle();
                mat->handle = materialHandle; // assign handle

                AssetMetaData materialMD;
                materialMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(materialBinFullPath); // relative path
                // materialMD.filepath = materialBinFullPath;
                materialMD.type = AssetType::Material;

                Project::GetInstance()->GetAssetManager().AssignAsset(materialHandle, mat);
                Project::GetInstance()->GetAssetManager().AssignMetaData(materialHandle, materialMD);
			}

			asset = CreateRef<StaticMesh>();
			for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
			{
                // Resolve material handle
                const int matIdx = meshScene.materialMap[(int)meshIdx];
                Ref<Material> mat = meshScene.materials[matIdx];

                Ref<MeshInstance> m = meshScene.flatMeshes[meshIdx];
                m->SetMaterial(mat->handle);

				asset->AddMeshInstance(m);
			}

			BinarySerializer::SerializeStaticMesh(asset, meshBinaryFullpath);
        }

        if (asset)
        {
            asset->handle = handle;

            auto relativePath = Project::GetInstance()->GetAssetRelativeFilepath(meshBinaryFullpath);
        }

        return asset;
	}

	Ref<Material> AssetImporter::ImportMaterial(AssetHandle handle, const AssetMetaData &metadata)
	{
        Ref<Material> asset;

        if (asset)
        {
            asset->handle = handle;
        }
        return asset;
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
		TextureCreateInfo createInfo;
		createInfo.format = nvrhi::Format::RGBA8_UNORM;
		createInfo.mipLevels = 4;
		createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
		createInfo.keepInitialState = true; // should keep initial state

        Ref<Texture> result;
        
        // Create texture on main thread, submit upload via render thread
        nvrhi::CommandListHandle cmd = Application::GetGraphicsDevice()->createCommandList();
        cmd->open();
		result = Texture::Create(metadata.filepath, createInfo, cmd);
        cmd->close();
        
        // Submit to render thread's queue (thread-safe)
        Application::SubmitWorkerCommandList(cmd);

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
