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
        static void AppendRaw(std::vector<std::byte> &out, const T &value)
        {
            const std::byte *raw = reinterpret_cast<const std::byte *>(&value);
            out.insert(out.end(), raw, raw + sizeof(T));
        }

        static std::vector<std::byte> SerializeMaterial(const Ref<Material> &mat, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            nvrhi::IDevice *device = Application::GetGraphicsDevice();
            nvrhi::CommandListHandle cmd = device->createCommandList();

            // Write name
            std::string nameCopy = mat->name;
            nameCopy += '\0';
            uint32_t nameSize = static_cast<uint32_t>(nameCopy.size());
            AppendRaw(buffer, nameSize);

            buffer.insert(buffer.end(),
                reinterpret_cast<const std::byte *>(nameCopy.data()),
                reinterpret_cast<const std::byte *>(nameCopy.data()) + nameSize
            );

            MaterialType matType = mat->GetType();
            AppendRaw(buffer, matType);

            // NOTE: WRITE COMPRESSED TEXTURE DATA (PNG)
            // Should use AssetHandle instead
            auto writeTextureFunc = [&](Ref<Texture> texture)
            {
                const TextureCreateInfo &texCreateInfo = texture->GetCreateInfo();

				size_t width = static_cast<size_t>(texture->GetWidth());
				size_t height = static_cast<size_t>(texture->GetHeight());
				size_t rowPitch = 0;

				// Map and read the pixel data
				void *pixelData = Texture::GetPixelData(texture, &rowPitch, cmd, device);

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

				// Write texture create info
				AppendRaw(buffer, texCreateInfo);
				
			    // Write compressed data size and compressed data
			    uint32_t compressedSize = static_cast<uint32_t>(compressedData.size());
			    AppendRaw(buffer, compressedSize);

			    const std::byte *begin = reinterpret_cast<const std::byte *>(compressedData.data());
			    buffer.insert(buffer.end(), begin, begin + compressedSize);
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
            inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
            std::vector<char> stringBytes(nameSize); // owns the bytes
            inFile.read(stringBytes.data(), nameSize);
            mat->name = std::string(stringBytes.data());

            // Read type
            MaterialType matType;
            inFile.read(reinterpret_cast<char *>(&matType), sizeof(matType));
            mat->SetType(matType);

            // Read texture
            auto readTextureFunc = [&]() -> Ref<Texture>
            {
                TextureCreateInfo texCreateInfo;
				inFile.read(reinterpret_cast<char *>(&texCreateInfo), sizeof(texCreateInfo));

				// Read compressed data size
				uint32_t compressedSize = 0;
				inFile.read(reinterpret_cast<char *>(&compressedSize), sizeof(compressedSize));

				// Read compressed data
				std::vector<unsigned char> compressedData(compressedSize);
				inFile.read(reinterpret_cast<char *>(compressedData.data()), compressedSize);

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
            };

			mat->baseColorTexture = readTextureFunc();
			mat->emissiveTexture = readTextureFunc();
			mat->metallicRoughnessTexture = readTextureFunc();
			mat->normalTexture = readTextureFunc();
			mat->occlusionTexture = readTextureFunc();

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
				buffer.insert
                (
                    buffer.end(),
					reinterpret_cast<const std::byte *>(primitive->indices.data()),
					reinterpret_cast<const std::byte *>(primitive->indices.data()) + indicesCount * sizeof(uint32_t)
				);

				// Write name
				std::string nameCopy = m->GetName();
				nameCopy += '\0';
				// name size in bytes including null-terminated
				uint32_t nameSize = static_cast<uint32_t>(nameCopy.size());
				AppendRaw(buffer, nameSize);

				buffer.insert(buffer.end(),
					reinterpret_cast<const std::byte *>(nameCopy.data()),
					reinterpret_cast<const std::byte *>(nameCopy.data() + nameSize)
				);

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
            inFile.read(reinterpret_cast<char *>(&meshCount), sizeof(meshCount));

            for (uint32_t i = 0; i < meshCount; ++i)
            {
                uint32_t verticesCount = 0, indicesCount = 0;
                inFile.read(reinterpret_cast<char *>(&verticesCount), sizeof(verticesCount));
                inFile.read(reinterpret_cast<char *>(&indicesCount), sizeof(indicesCount));

                Ref<MeshInstance> meshInstance = CreateRef<MeshInstance>();
                auto &name = meshInstance->GetName();
                auto &primitive = meshInstance->GetPrimitive();
            
                // Read vertices
                primitive->vertices.reserve(verticesCount);
				for (uint32_t vertexIndex = 0; vertexIndex < verticesCount; ++vertexIndex)
				{
					VertexMesh_Anim vertex;
					inFile.read(reinterpret_cast<char *>(&vertex.position), sizeof(vertex.position));
					inFile.read(reinterpret_cast<char *>(&vertex.normal), sizeof(vertex.normal));
					inFile.read(reinterpret_cast<char *>(&vertex.tangent), sizeof(vertex.tangent));
					inFile.read(reinterpret_cast<char *>(&vertex.bitangent), sizeof(vertex.bitangent));
					inFile.read(reinterpret_cast<char *>(&vertex.uv), sizeof(vertex.uv));

					inFile.read(reinterpret_cast<char *>(vertex.boneIDs), sizeof(vertex.boneIDs));
					inFile.read(reinterpret_cast<char *>(vertex.weights), sizeof(vertex.weights));

                    primitive->vertices.push_back(vertex);
				}

                // Read indices
                primitive->indices.resize(indicesCount);
				inFile.read(reinterpret_cast<char *>(primitive->indices.data()), indicesCount * sizeof(uint32_t));

				// Read name
				uint32_t nameSize = 0;
				inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
				std::vector<char> stringBytes(nameSize); // owns the bytes
				inFile.read(stringBytes.data(), nameSize);
				name = std::string(stringBytes.data());

				// Read local transform
				for (int i = 0; i < 4; ++i)
				{
					inFile.read(reinterpret_cast<char *>(&meshInstance->local[i].x), sizeof(float));
					inFile.read(reinterpret_cast<char *>(&meshInstance->local[i].y), sizeof(float));
					inFile.read(reinterpret_cast<char *>(&meshInstance->local[i].z), sizeof(float));
					inFile.read(reinterpret_cast<char *>(&meshInstance->local[i].w), sizeof(float));
				}

                // Read material
                uint64_t materialHandle = 0;
                inFile.read(reinterpret_cast<char *>(&materialHandle), sizeof(materialHandle));
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
            std::string nameCopy = anim->name;
            nameCopy += '\0';
            uint32_t nameSize = static_cast<uint32_t>(nameCopy.size());
            AppendRaw(buffer, nameSize);

            buffer.insert(buffer.end(),
                reinterpret_cast<const std::byte *>(nameCopy.data()),
                reinterpret_cast<const std::byte *>(nameCopy.data() + nameSize)
            );

            // write channels
            uint32_t channelCount = static_cast<uint32_t>(anim->channels.size());
            AppendRaw(buffer, channelCount);

            for (const auto &[channelName, channel] : anim->channels)
            {
                // write uint32_t name size
                nameCopy = channelName;
                nameCopy += '\0';
                uint32_t channelNameSize = static_cast<uint32_t>(nameCopy.size());
                AppendRaw(buffer, channelNameSize);

                buffer.insert(buffer.end(),
                    reinterpret_cast<const std::byte *>(nameCopy.data()),
                    reinterpret_cast<const std::byte *>(nameCopy.data() + channelNameSize));

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
            inFile.read(reinterpret_cast<char *>(&anim->duration), sizeof(anim->duration));
            inFile.read(reinterpret_cast<char *>(&anim->ticksPerSeconds), sizeof(anim->ticksPerSeconds));

            // read name size and name
            uint32_t nameSize = 0;
            inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
            std::vector<char> stringBytes(nameSize); // owns the bytes
            inFile.read(stringBytes.data(), nameSize);
            anim->name = std::string(stringBytes.data());

            // read channel count
            uint32_t channelCount = 0;
            inFile.read(reinterpret_cast<char *>(&channelCount), sizeof(channelCount));

            anim->channels.reserve(channelCount);

            for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx)
            {
                // read channel name
                inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
                stringBytes.resize(nameSize);
                inFile.read(stringBytes.data(), nameSize);
                std::string channelName = std::string(stringBytes.data());

                AnimationChannel channel{};


                // read total frame data in bytes (translation + rotation + scale) for validation
                uint32_t expectedTotalSize = 0;
                inFile.read(reinterpret_cast<char *>(&expectedTotalSize), sizeof(expectedTotalSize));
                uint32_t totalChannelByteSize = 0;

                // process translation keys
                uint32_t translationFrameCount = 0;
                inFile.read(reinterpret_cast<char *>(&translationFrameCount), sizeof(translationFrameCount));

                channel.translationKeys.frames.reserve(translationFrameCount);
                for (uint32_t frameIdx = 0; frameIdx < translationFrameCount; ++frameIdx)
                {
                    KeyFrame<glm::vec3> frame{};

                    inFile.read(reinterpret_cast<char *>(&frame.Value.x), sizeof(frame.Value));
                    inFile.read(reinterpret_cast<char *>(&frame.Timestamp), sizeof(frame.Timestamp));
                    totalChannelByteSize += sizeof(frame);

                    channel.translationKeys.frames.push_back(frame);
                }

                // process rotation keys
                uint32_t rotationFrameCount = 0;
                inFile.read(reinterpret_cast<char *>(&rotationFrameCount), sizeof(rotationFrameCount));

                channel.rotationKeys.frames.reserve(rotationFrameCount);
                for (uint32_t frameIdx = 0; frameIdx < rotationFrameCount; ++frameIdx)
                {
                    KeyFrame<glm::quat> frame{};

                    inFile.read(reinterpret_cast<char *>(&frame.Value.x), sizeof(frame.Value));
                    inFile.read(reinterpret_cast<char *>(&frame.Timestamp), sizeof(frame.Timestamp));
                    totalChannelByteSize += sizeof(frame);

                    channel.rotationKeys.frames.push_back(frame);
                }

                // process scale keys
                uint32_t scaleFrameCount = 0;
                inFile.read(reinterpret_cast<char *>(&scaleFrameCount), sizeof(scaleFrameCount));

                channel.scaleKeys.frames.reserve(scaleFrameCount);
                for (uint32_t frameIdx = 0; frameIdx < scaleFrameCount; ++frameIdx)
                {
                    KeyFrame<glm::vec3> frame{};

                    inFile.read(reinterpret_cast<char *>(&frame.Value.x), sizeof(frame.Value));
                    inFile.read(reinterpret_cast<char *>(&frame.Timestamp), sizeof(frame.Timestamp));
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

            buffer.insert(
                buffer.end(),
                reinterpret_cast<const std::byte *>(stringTable.data()),
                reinterpret_cast<const std::byte *>(stringTable.data() + stringSize)
            );

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
            inFile.read(reinterpret_cast<char *>(&jointCount), sizeof(uint32_t));

            // read disk joint array
            std::vector<DiskJoint> diskJoints(jointCount);
            inFile.read(reinterpret_cast<char *>(diskJoints.data()), sizeof(DiskJoint) * jointCount);

            // read string table
            uint32_t stringTableSize = 0;
            inFile.read(reinterpret_cast<char *>(&stringTableSize), sizeof(stringTableSize));

            std::vector<char> stringTable(stringTableSize); // owns the bytes
            inFile.read(stringTable.data(), stringTableSize);

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
