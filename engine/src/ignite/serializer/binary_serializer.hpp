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

#include <stb_image.h>
#include <stb_image_write.h>
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

#include <filesystem>
#include <vector>
#include <cinttypes>
#include <fstream>
#include <cstdlib>

namespace ignite
{
    struct FileHeader
    {
        char engineVersion[4]; // 4 B OFFSET 0
        char magic[4];         // 4 B OFFSET 4 - identifier (e.g SKEL for skeleton)
        char fileFormat[3];    // 3 B OFFSET 8 - major.minor.revision
        uint8_t compression;   // 1 B OFFSET 10 - compression flag (0 = none, 1 = zlib)
        // ^ 12 bytes

        uint64_t payloadSize;   // 8 B OFFSET 12 - uncompressed size, Fast sanity check, file length
        char userDetails[36];   // 36 B OFFSET 20 - e.g artist name, tool version, date
    };

    struct DiskJoint
    {
        uint32_t nameOffset;          // into the string table
        int32_t  id;
        int32_t  parentId;
        float    inverseBindPose[16]; // column‑major 4×4
        float    localTransform[16]; // column‑major 4×4
    };

    class BinarySerializer
    {
    public:
        template<typename T>
        static void AppendRaw(std::vector<std::byte> &out, const T &value, size_t sizeInBytes = 0)
        {
            const std::byte *raw = reinterpret_cast<const std::byte *>(&value);
            const size_t bytesToCopy = sizeInBytes ? sizeInBytes : sizeof(T);
            const size_t oldSize = out.size();
            out.resize(oldSize + bytesToCopy);
            std::memcpy(out.data() + oldSize, raw, bytesToCopy);
        }

		static void AppendBytes(std::vector<std::byte> &out, const void *data, size_t sizeInBytes)
		{
			const auto *raw = static_cast<const std::byte *>(data);
			const size_t oldSize = out.size();
			out.resize(oldSize + sizeInBytes);
			std::memcpy(out.data() + oldSize, raw, sizeInBytes);
		}

		static void AppendString(std::vector<std::byte> &out, const std::string &str, uint32_t &outStrSize)
        {
			std::string strCopy = str;
            strCopy += '\0';
			
            outStrSize = static_cast<uint32_t>(strCopy.size());
			AppendRaw(out, outStrSize);

			AppendBytes(out, strCopy.data(), outStrSize);
        }

        template<typename T>
        static void ReadRaw(std::ifstream &stream, T *outData, size_t sizeInBytes = 0)
        {
            stream.read(reinterpret_cast<char *>(outData), sizeInBytes ? sizeInBytes : sizeof(T));
        }

        static std::string ReadString(std::ifstream &stream, uint32_t strSize)
        {
            std::vector<char> stringBytes(strSize); // owns the bytes
            stream.read(stringBytes.data(), strSize);

            std::string result = std::string(stringBytes.data());
            return result;
        }

        static std::vector<std::byte> SerializeMaterial(const Ref<Material> &mat, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            // Write name
            uint32_t strSize = 0;
            AppendString(buffer, mat->name, strSize);

            MaterialType matType = mat->GetType();
            AppendRaw(buffer, matType);

            // NOTE: WRITE COMPRESSED TEXTURE DATA (PNG)
            // Should use AssetHandle instead
            auto writeTextureFunc = [&](Ref<Texture> texture)
            {

				const TextureCreateInfo &texCreateInfo = texture->GetCreateInfo();
				// Write texture create info
				AppendRaw(buffer, texCreateInfo);

				bool hasTexture = texture->GetBuffer(); // store flag

                if (!hasTexture)
                {
                    AppendRaw(buffer, hasTexture);
                }
                else
                {
					AppendRaw(buffer, hasTexture);

					size_t width = static_cast<size_t>(texture->GetWidth());
					size_t height = static_cast<size_t>(texture->GetHeight());
					size_t rowPitch = width * texture->GetChannels();

					// Map and read the pixel data
					void *pixelData = texture->GetBuffer().data; 

					// Compress pixel data to PNG format
					// Use stbi_write_png_to_func with a custom callback to write to memory
					std::vector<unsigned char> compressedData;

					auto writeCallback = [](void *context, void *data, int size)
					{
						auto *vec = static_cast<std::vector<unsigned char> *>(context);
						const unsigned char *bytes = static_cast<const unsigned char *>(data);
						vec->insert(vec->end(), bytes, bytes + size);
					};

					stbi_write_png_to_func(
						writeCallback,
						&compressedData,
						static_cast<int>(width),
						static_cast<int>(height),
						4, // RGBA = 4 channels
						pixelData,
						static_cast<int>(rowPitch)
					);

					// Write compressed data size and compressed data
					uint32_t compressedSize = static_cast<uint32_t>(compressedData.size());
					AppendRaw(buffer, compressedSize);

					AppendBytes(buffer, compressedData.data(), compressedSize);
                }
            };
            
            writeTextureFunc(mat->baseColorTexture);
            writeTextureFunc(mat->emissiveTexture);
            writeTextureFunc(mat->metallicRoughnessTexture);
            writeTextureFunc(mat->normalTexture);
            writeTextureFunc(mat->occlusionTexture);

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();
      
            return buffer;
        }

        static Ref<Material> DeserializeMaterial(const std::filesystem::path &filepath)
        {
            Ref<Material> mat = CreateRef<Material>();
            std::ifstream inFile(filepath, std::ios::binary);

            if (!inFile)
            {
                LOG_ASSERT("Filed does not exists {0}", filepath.generic_string());
                return nullptr;
            }

            // Read name
            uint32_t nameSize = 0;
            ReadRaw(inFile, &nameSize);
            
            mat->name = ReadString(inFile, nameSize);

            // Read type
            MaterialType matType;
			ReadRaw(inFile, &matType);
            mat->SetType(matType);

            // Read texture
            auto readTextureFunc = [&](Ref<Texture> fallbackTexture) -> Ref<Texture>
            {
				TextureCreateInfo texCreateInfo;
				ReadRaw(inFile, &texCreateInfo);

                bool hasTexture = false;
                ReadRaw(inFile, &hasTexture);

                if (hasTexture)
                {
				    // Read compressed data size
				    uint32_t compressedSize = 0;
                    ReadRaw(inFile, &compressedSize);

				    // Read compressed data
				    std::vector<unsigned char> compressedData(compressedSize);
				    ReadRaw(inFile, compressedData.data(), compressedSize);

				    // Decompress using stbi
				    int width, height, channels;
				    unsigned char *decompressedData = stbi_load_from_memory(
					    compressedData.data(),
					    static_cast<int>(compressedSize),
					    &width,
					    &height,
					    &channels,
					    4 // force RGBA
				    );

				    if (!decompressedData)
				    {
					    LOG_ASSERT(false, "Failed to decompress texture data");
					    return nullptr;
				    }

				    // Calculate the size needed for Buffer
				    const size_t pixelBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
				    Buffer buffer(pixelBytes);
				    std::memcpy(buffer.data, decompressedData, pixelBytes);

				    // Free decompressed data
				    stbi_image_free(decompressedData);

                    auto tex = Texture::Create(buffer, texCreateInfo, nullptr);
                    return tex;
                }
                else
                {
                    return fallbackTexture;
                }
            };

			mat->baseColorTexture = readTextureFunc(Renderer::GetWhiteTexture());
			mat->emissiveTexture = readTextureFunc(Renderer::GetBlackTexture());
			mat->metallicRoughnessTexture = readTextureFunc(Renderer::GetBlackTexture());
			mat->normalTexture = readTextureFunc(Renderer::GetWhiteTexture());
			mat->occlusionTexture = readTextureFunc(Renderer::GetWhiteTexture());

            inFile.close();
            return mat;
        }

        static std::vector<std::byte> SerializeStaticMesh(const Ref<StaticMesh> &sm, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;
            const std::vector<Ref<MeshInstance>> &meshInstances = sm->GetMeshInstances();

            // write mesh vector
            uint32_t meshCount = static_cast<uint32_t>(meshInstances.size());
            AppendRaw(buffer, meshCount);

            for (auto &m : meshInstances)
            {
                auto &primitive = m->GetPrimitive();
                
                // Write vertices and indices count
                uint32_t verticesCount = static_cast<uint32_t>(primitive->vertices.size());
                uint32_t indicesCount = static_cast<uint32_t>(primitive->indices.size());
                AppendRaw(buffer, verticesCount);
                AppendRaw(buffer, indicesCount);

				// Write vertices
				for (VertexMesh_Anim &vertex : primitive->vertices)
				{
					AppendRaw(buffer, vertex.position);
					AppendRaw(buffer, vertex.normal);
					AppendRaw(buffer, vertex.tangent);
					AppendRaw(buffer, vertex.bitangent);
					AppendRaw(buffer, vertex.uv);

					AppendRaw(buffer, vertex.boneIDs);
					AppendRaw(buffer, vertex.weights);
				}

				// write indices
				AppendBytes(buffer, primitive->indices.data(), indicesCount * sizeof(uint32_t));

				// Write name
                uint32_t nameSize = 0;
                AppendString(buffer, m->GetName(), nameSize);

				// Write local transform
				for (int i = 0; i < 4; ++i)
				{
					AppendRaw(buffer, m->local[i].x);
					AppendRaw(buffer, m->local[i].y);
					AppendRaw(buffer, m->local[i].z);
					AppendRaw(buffer, m->local[i].w);
				}

                // Write material reference
                uint64_t materialHandle = m->GetMaterialHandle();
                AppendRaw(buffer, materialHandle);
            }

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<StaticMesh> DeserializeStaticMesh(const std::filesystem::path &filepath)
        {
            Ref<StaticMesh> staticMesh = CreateRef<StaticMesh>();

            std::ifstream inFile(filepath, std::ios::binary);

            if (!inFile)
            {
                return nullptr;
            }

            // read mesh vector
            uint32_t meshCount = 0;
            ReadRaw(inFile, &meshCount);

            for (uint32_t i = 0; i < meshCount; ++i)
            {
                uint32_t verticesCount = 0, indicesCount = 0;
                ReadRaw(inFile, &verticesCount);
                ReadRaw(inFile, &indicesCount);

                Ref<MeshInstance> meshInstance = CreateRef<MeshInstance>();
                auto &name = meshInstance->GetName();
                auto &primitive = meshInstance->GetPrimitive();
            
                // Read vertices
                primitive->vertices.reserve(verticesCount);
				for (uint32_t vertexIndex = 0; vertexIndex < verticesCount; ++vertexIndex)
				{
					VertexMesh_Anim vertex;
					ReadRaw(inFile, &vertex.position);
					ReadRaw(inFile, &vertex.normal);
					ReadRaw(inFile, &vertex.tangent);
					ReadRaw(inFile, &vertex.bitangent);
					ReadRaw(inFile, &vertex.uv);

					ReadRaw(inFile, &vertex.boneIDs);
					ReadRaw(inFile, &vertex.weights);

                    primitive->vertices.push_back(vertex);
				}

                // Read indices
                primitive->indices.resize(indicesCount);
                ReadRaw(inFile, primitive->indices.data(), indicesCount * sizeof(uint32_t));

                // Read name
				uint32_t nameSize = 0;
				ReadRaw(inFile, &nameSize);
                name = ReadString(inFile, nameSize);

				// Read local transform
				for (int i = 0; i < 4; ++i)
				{
					ReadRaw(inFile, &meshInstance->local[i].x);
					ReadRaw(inFile, &meshInstance->local[i].y);
					ReadRaw(inFile, &meshInstance->local[i].z);
                    ReadRaw(inFile, &meshInstance->local[i].w);
				}

                // Read material
                uint64_t materialHandle = 0;
                ReadRaw(inFile, &materialHandle);
                if (materialHandle != 0)
                {
                    meshInstance->SetMaterial(AssetHandle(materialHandle));
                }

				// Pushback
                staticMesh->AddMeshInstance(meshInstance);
            }

            inFile.close();
            return staticMesh;
        }

        static std::vector<std::byte> SerializeAnimation(const Ref<SkeletalAnimation> &anim, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            // write animation duration and ticks per second
            AppendRaw(buffer, anim->duration);
            AppendRaw(buffer, anim->ticksPerSeconds);

            // write name size and name

            uint32_t nameSize = 0;
            AppendString(buffer, anim->name, nameSize);

            // write channels
            uint32_t channelCount = static_cast<uint32_t>(anim->channels.size());
            AppendRaw(buffer, channelCount);

            for (const auto &[channelName, channel] : anim->channels)
            {
                AppendString(buffer, channelName, nameSize);

                uint32_t translationFrameCount = static_cast<uint32_t>(channel.translationKeys.frames.size());
                uint32_t rotationFrameCount = static_cast<uint32_t>(channel.rotationKeys.frames.size());
                uint32_t scaleFrameCountFrameCount = static_cast<uint32_t>(channel.scaleKeys.frames.size());

                // write total frame data size for validation
                uint32_t framesDataSize = 
                      translationFrameCount * sizeof(KeyFrame<glm::vec3>)
                    + rotationFrameCount * sizeof(KeyFrame<glm::quat>)
                    + scaleFrameCountFrameCount * sizeof(KeyFrame<glm::vec3>);

                AppendRaw(buffer, framesDataSize);

                // Write Translation frames
                AppendRaw(buffer, translationFrameCount);

                for (const auto &frame : channel.translationKeys.frames)
                {
                    AppendRaw(buffer, frame.Value);
                    AppendRaw(buffer, frame.Timestamp);
                }

                // Write Rotation frames
                AppendRaw(buffer, rotationFrameCount);

                for (const auto &frame : channel.rotationKeys.frames)
                {
                    AppendRaw(buffer, frame.Value);
                    AppendRaw(buffer, frame.Timestamp);
                }

                // Write Scale frames
                AppendRaw(buffer, scaleFrameCountFrameCount);

                for (const auto &frame : channel.scaleKeys.frames)
                {
                    AppendRaw(buffer, frame.Value);
                    AppendRaw(buffer, frame.Timestamp);
                }
            }

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<SkeletalAnimation> DeserializeAnimation(const std::filesystem::path &filepath)
        {
            Ref<SkeletalAnimation> anim = CreateRef<SkeletalAnimation>();

            std::ifstream inFile(filepath, std::ios::binary);

            if (!inFile)
            {
                throw std::runtime_error("Cannot open animation file " + filepath.string());
            }

            // read animation duration and ticks per second
            ReadRaw(inFile, &anim->duration);
            ReadRaw(inFile, &anim->ticksPerSeconds);

            // read name size and name
            uint32_t nameSize = 0;
            ReadRaw(inFile, &nameSize);
            anim->name = ReadString(inFile, nameSize);

            // read channel count
            uint32_t channelCount = 0;
            ReadRaw(inFile, &channelCount);

            anim->channels.reserve(channelCount);

            for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx)
            {
                // read channel name
				ReadRaw(inFile, &nameSize);
                std::string channelName = ReadString(inFile, nameSize);

                AnimationChannel channel{};

                // read total frame data in bytes (translation + rotation + scale) for validation
                uint32_t expectedTotalSize = 0;
                ReadRaw(inFile, &expectedTotalSize);

                uint32_t totalChannelByteSize = 0;

                // process translation keys
                uint32_t translationFrameCount = 0;
                ReadRaw(inFile, &translationFrameCount);

                channel.translationKeys.frames.reserve(translationFrameCount);
                for (uint32_t frameIdx = 0; frameIdx < translationFrameCount; ++frameIdx)
                {
                    KeyFrame<glm::vec3> frame{};

					ReadRaw(inFile, &frame.Value);
					ReadRaw(inFile, &frame.Timestamp);

                    totalChannelByteSize += sizeof(frame);

                    channel.translationKeys.frames.push_back(frame);
                }

                // process rotation keys
                uint32_t rotationFrameCount = 0;
				ReadRaw(inFile, &rotationFrameCount);

                channel.rotationKeys.frames.reserve(rotationFrameCount);
                for (uint32_t frameIdx = 0; frameIdx < rotationFrameCount; ++frameIdx)
                {
                    KeyFrame<glm::quat> frame{};

					ReadRaw(inFile, &frame.Value);
					ReadRaw(inFile, &frame.Timestamp);

                    totalChannelByteSize += sizeof(frame);

                    channel.rotationKeys.frames.push_back(frame);
                }

                // process scale keys
                uint32_t scaleFrameCount = 0;
                ReadRaw(inFile, &scaleFrameCount);

                channel.scaleKeys.frames.reserve(scaleFrameCount);
                for (uint32_t frameIdx = 0; frameIdx < scaleFrameCount; ++frameIdx)
                {
                    KeyFrame<glm::vec3> frame{};

					ReadRaw(inFile, &frame.Value);
					ReadRaw(inFile, &frame.Timestamp);

                    totalChannelByteSize += sizeof(frame);

                    channel.scaleKeys.frames.push_back(frame);
                }

                LOG_ASSERT(expectedTotalSize == totalChannelByteSize,
                    "Corrupt animation data expected channel size {}, got {}",
                    expectedTotalSize, totalChannelByteSize);

                anim->channels[channelName] = channel;
            }

            inFile.close();

            return anim;
        }

        static std::vector<std::byte> SerializeSkeleton(const Ref<Skeleton> &skeleton, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            // write joint count
            uint32_t jointCount = static_cast<uint32_t>(skeleton->joints.size());
            AppendRaw(buffer, jointCount);

            // build the string table and map: joint name -> offset in string table
            std::string stringTable;
            std::unordered_map<std::string, uint32_t> nameOffsets;

            for (const Joint &joint : skeleton->joints)
            {
                if (!nameOffsets.contains(joint.name))
                {
                    uint32_t offset = static_cast<uint32_t>(stringTable.size());
                    nameOffsets[joint.name] = offset;
                    stringTable += joint.name;
                    stringTable += '\0'; // Null-terminate
                }
            }

            for (const Joint &joint : skeleton->joints)
            {
                DiskJoint dj{};
                dj.nameOffset = nameOffsets[joint.name];
                dj.id = joint.id;
                dj.parentId = joint.parentJointId;

                // store inverse bindpose in column-major order
                std::memcpy(dj.inverseBindPose, &joint.inverseBindPose[0].x, sizeof(float) * 16);
                std::memcpy(dj.localTransform, &joint.localTransform[0].x, sizeof(float) * 16);

                AppendRaw(buffer, dj);
            }

            // write string table size and data
            uint32_t stringSize = static_cast<uint32_t>(stringTable.size());
            AppendRaw(buffer, stringSize);

			AppendBytes(buffer, stringTable.data(), stringSize);

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<Skeleton> DeserializeSkeleton(const std::filesystem::path &filepath)
        {
            Ref<Skeleton> skeleton = CreateRef<Skeleton>();

            std::ifstream inFile(filepath, std::ios::binary);

            if (!inFile)
            {
                throw std::runtime_error("Cannot open skeleton file " + filepath.string());
            }

            // read joint count
            uint32_t jointCount = 0;
            ReadRaw(inFile, &jointCount);

            // read disk joint array
            std::vector<DiskJoint> diskJoints(jointCount);
			ReadRaw(inFile, diskJoints.data(), sizeof(DiskJoint) * jointCount);

            // read string table
            uint32_t stringTableSize = 0;
            ReadRaw(inFile, &stringTableSize);

            std::vector<char> stringTable(stringTableSize); // owns the bytes
            ReadRaw(inFile, stringTable.data(), stringTableSize);

            skeleton->joints.reserve(jointCount);

            for (const DiskJoint &dj : diskJoints)
            {
                if (dj.nameOffset >= stringTableSize)
                {
                    throw std::runtime_error("Corrupt file: nameOffset out of range");
                }

                const char *namePtr = stringTable.data() + dj.nameOffset;
                std::string jointName = std::string(namePtr);

                Joint joint{};
                joint.name = jointName;
                joint.id = dj.id;
                joint.parentJointId = dj.parentId;
                joint.globalTransform = glm::mat4(1.0f);

                std::memcpy(&joint.inverseBindPose[0].x, dj.inverseBindPose, sizeof(float) * 16);
                std::memcpy(&joint.localTransform[0].x, dj.localTransform, sizeof(float) * 16);

                skeleton->joints.push_back(std::move(joint));

                skeleton->nameToJointMap[jointName] = joint.id;
            }

            inFile.close();

            return skeleton;
        }
    };
}
