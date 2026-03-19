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
#include "ignite/graphics/gpu_upload_sync.hpp"
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
                    Application::SubmitToRenderThread([m = mesh]()
                    {
                        nvrhi::IDevice* device = Application::GetGraphicsDevice();
                        nvrhi::CommandListHandle cmd = device->createCommandList();
                        cmd->open();
					    m->GetPrimitive()->CreateBuffer(cmd);
                        cmd->close();
                            
                        Application::SubmitWorkerCommandList(cmd);
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
            MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene);

            // Prepare AssetHandle map for texture material textures
            // we need 5 textures
            std::vector<std::array<AssetHandle, 5>> materialTextureHandles;
            materialTextureHandles.resize(meshScene.materials.size());

            // Import and store textures
            for (size_t i = 0; i < meshScene.materialTextureMap.size(); ++i)
            {
                auto &textureHandles = materialTextureHandles[i];

                // Set the default value to be 0
                std::fill(textureHandles.begin(), textureHandles.end(), AssetHandle(0));

                for (size_t j = 0; j < meshScene.materialTextureMap[i].size(); ++j)
                {
                    auto &[idx, texture] = meshScene.materialTextureMap[i][j];

                    // no texture
                    if (idx < 0)
                        continue;

					const std::string textureFilename = filename.stem().string() + std::format("_{0}_{1}", idx, ".png");
					std::filesystem::path texturePNGFullPath = textureDirectory / textureFilename;
					BinarySerializer::SerializeTextureToPNG(texture, texturePNGFullPath);

					AssetHandle textureHandle = AssetHandle();
					texture->handle = textureHandle;

					AssetMetaData textureMD;
					textureMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(texturePNGFullPath);
					textureMD.type = AssetType::Texture;

					Project::GetInstance()->GetAssetManager().AssignAsset(textureHandle, texture);
					Project::GetInstance()->GetAssetManager().AssignMetaData(textureHandle, textureMD);

                    // If the texture has been stored, then assign AssetHandle
                    textureHandles[j] = textureHandle;
                }
            }

            // Import and store material
			for (size_t i = 0; i < meshScene.materials.size(); ++i)
			{
                Ref<Material> &mat = meshScene.materials[i];

                // First store the texture handles
				mat->baseColorTextureHandle = materialTextureHandles[i][0];
				mat->emissiveTextureHandle = materialTextureHandles[i][1];
				mat->metallicRoughnessTextureHandle = materialTextureHandles[i][2];
				mat->normalTextureHandle = materialTextureHandles[i][3];
				mat->occlusionTextureHandle = materialTextureHandles[i][4];

                const std::string materialFilename = mat->name+materialBinExt;
                std::filesystem::path materialBinFullPath = materialDirectory / materialFilename;
                BinarySerializer::SerializeMaterial(mat, materialBinFullPath);

                AssetHandle materialHandle = AssetHandle();
                mat->handle = materialHandle; // assign material handle

                AssetMetaData materialMD;
                materialMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(materialBinFullPath);
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

            // Serialize the mesh
			BinarySerializer::SerializeStaticMesh(asset, meshBinaryFullpath);
        }

        if (asset)
        {
            asset->handle = handle;

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
					Application::SubmitToRenderThread([m = mesh]()
						{
							nvrhi::IDevice *device = Application::GetGraphicsDevice();
							nvrhi::CommandListHandle cmd = device->createCommandList();
							cmd->open();
							m->GetPrimitive()->CreateBuffer(cmd);
							cmd->close();

							Application::SubmitWorkerCommandList(cmd);
						});
				}
			}

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
        createInfo.deferGpuCreate = true;

        // Load texture pixel data on worker thread (no GPU operations)
        Ref<Texture> result = Texture::Create(metadata.filepath, createInfo, nullptr);

        // Submit GPU upload to main thread with proper synchronization
        Application::SubmitToRenderThread([texture = result]()
        {
            nvrhi::CommandListHandle cmd = Application::GetGraphicsDevice()->createCommandList();
            cmd->open();
            texture->SetData(cmd, 4);
            cmd->close();
            
            Application::SubmitWorkerCommandList(cmd);
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
