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
#include <openexr.h>
#include <openexr_errors.h>
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstring>
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

    struct DiskSocket
    {
        uint32_t nameOffset;
        int32_t parentId;
        float translation[3];
        float rotation[4]; // x, y, z, w
        float scale[3];
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
        static bool ReadRaw(std::ifstream &stream, T *outData, size_t sizeInBytes = 0)
        {
            if (stream.peek() != std::ifstream::traits_type::eof())
            {
                stream.read(reinterpret_cast<char *>(outData), sizeInBytes ? sizeInBytes : sizeof(T));
                return true;
            }
            return false;
        }

        static std::string ReadString(std::ifstream &stream, uint32_t strSize)
        {
            std::vector<char> stringBytes(strSize); // owns the bytes
            stream.read(stringBytes.data(), strSize);

            std::string result = std::string(stringBytes.data());
            return result;
        }


        static bool SerializeTextureToPNG(const Ref<Texture> &texture, const std::filesystem::path &filepath)
        {
            if (!texture)
                return false;

            const int channels = 4;
            const int width = static_cast<int>(texture->GetWidth());
            const int height = static_cast<int>(texture->GetHeight());

            if (width <= 0 || height <= 0)
                return false;

            const size_t bytesPerPixel = channels * sizeof(uint8_t);
            const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * bytesPerPixel;

            std::vector<uint8_t> pixelCopy;
            pixelCopy.resize(expectedSize);

            const Buffer &buffer = texture->GetBuffer();
            if (buffer.data && buffer.size >= expectedSize)
            {
                std::memcpy(pixelCopy.data(), buffer.data, expectedSize);
            }
            else
            {
                const std::filesystem::path &sourceFilepath = texture->GetFilepath();
                if (sourceFilepath.empty() || !std::filesystem::exists(sourceFilepath))
                    return false;

                int sourceWidth = 0;
                int sourceHeight = 0;
                int sourceChannels = 0;
                stbi_uc *sourceData = stbi_load(sourceFilepath.generic_string().c_str(), &sourceWidth, &sourceHeight, &sourceChannels, channels);
                if (!sourceData)
                    return false;

                if (sourceWidth != width || sourceHeight != height)
                {
                    stbi_image_free(sourceData);
                    return false;
                }

                std::memcpy(pixelCopy.data(), sourceData, expectedSize);
                stbi_image_free(sourceData);
            }

            int result = stbi_write_png(filepath.generic_string().c_str(), width, height, channels, pixelCopy.data(), width * channels);
            return result == 1;
        }

        static bool SerializeTextureToEXR(const Ref<Texture> &texture, const std::filesystem::path &filepath)
        {
            if (!texture)
            {
                return false;
            }

            const int width = static_cast<int>(texture->GetWidth());
            const int height = static_cast<int>(texture->GetHeight());
            if (width <= 0 || height <= 0)
            {
                return false;
            }

            const Buffer &buffer = texture->GetBuffer();
            if (!buffer.data || buffer.size == 0)
            {
                return false;
            }

            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<float> red(pixelCount, 0.0f);
            std::vector<float> green(pixelCount, 0.0f);
            std::vector<float> blue(pixelCount, 0.0f);
            std::vector<float> alpha(pixelCount, 1.0f);

            const auto fillFromFloatRGBA = [&]() -> bool
            {
                if (buffer.size < pixelCount * 4u * sizeof(float))
                {
                    return false;
                }

                const float *src = reinterpret_cast<const float *>(buffer.data);
                for (size_t i = 0; i < pixelCount; ++i)
                {
                    red[i] = src[i * 4u + 0u];
                    green[i] = src[i * 4u + 1u];
                    blue[i] = src[i * 4u + 2u];
                    alpha[i] = src[i * 4u + 3u];
                }
                return true;
            };

            const auto fillFromByteRGBA = [&]() -> bool
            {
                if (buffer.size < pixelCount * 4u)
                {
                    return false;
                }

                const uint8_t *src = buffer.data;
                for (size_t i = 0; i < pixelCount; ++i)
                {
                    red[i] = static_cast<float>(src[i * 4u + 0u]) / 255.0f;
                    green[i] = static_cast<float>(src[i * 4u + 1u]) / 255.0f;
                    blue[i] = static_cast<float>(src[i * 4u + 2u]) / 255.0f;
                    alpha[i] = static_cast<float>(src[i * 4u + 3u]) / 255.0f;
                }
                return true;
            };

            bool bufferDecoded = false;
            if (texture->GetFormat() == nvrhi::Format::RGBA32_FLOAT)
            {
                bufferDecoded = fillFromFloatRGBA();
            }
            else if (texture->GetFormat() == nvrhi::Format::RGBA8_UNORM)
            {
                bufferDecoded = fillFromByteRGBA();
            }
            else if (buffer.size >= pixelCount * 4u * sizeof(float))
            {
                bufferDecoded = fillFromFloatRGBA();
            }
            else if (buffer.size >= pixelCount * 4u)
            {
                bufferDecoded = fillFromByteRGBA();
            }

            if (!bufferDecoded)
            {
                return false;
            }

            exr_context_t ctx = nullptr;
            exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
            exr_result_t rv = exr_start_write(&ctx, filepath.string().c_str(), EXR_WRITE_FILE_DIRECTLY, &cinit);
            if (rv != EXR_ERR_SUCCESS)
            {
                return false;
            }

            bool success = false;
            exr_encode_pipeline_t encode = EXR_ENCODE_PIPELINE_INITIALIZER;
            bool encodeInitialized = false;

            do
            {
                int partIndex = 0;
                rv = exr_add_part(ctx, "Texture", EXR_STORAGE_SCANLINE, &partIndex);
                if (rv != EXR_ERR_SUCCESS)
                {
                    break;
                }

                rv = exr_initialize_required_attr_simple(ctx, partIndex, width, height, EXR_COMPRESSION_NONE);
                if (rv != EXR_ERR_SUCCESS)
                {
                    break;
                }

                rv = exr_add_channel(ctx, partIndex, "R", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1);
                if (rv != EXR_ERR_SUCCESS) break;
                rv = exr_add_channel(ctx, partIndex, "G", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1);
                if (rv != EXR_ERR_SUCCESS) break;
                rv = exr_add_channel(ctx, partIndex, "B", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LOGARITHMIC, 1, 1);
                if (rv != EXR_ERR_SUCCESS) break;
                rv = exr_add_channel(ctx, partIndex, "A", EXR_PIXEL_FLOAT, EXR_PERCEPTUALLY_LINEAR, 1, 1);
                if (rv != EXR_ERR_SUCCESS) break;

                rv = exr_write_header(ctx);
                if (rv != EXR_ERR_SUCCESS)
                {
                    break;
                }

                exr_chunk_info_t chunk{};
                rv = exr_write_scanline_chunk_info(ctx, partIndex, 0, &chunk);
                if (rv != EXR_ERR_SUCCESS)
                {
                    break;
                }

                rv = exr_encoding_initialize(ctx, partIndex, &chunk, &encode);
                if (rv != EXR_ERR_SUCCESS)
                {
                    break;
                }

                encodeInitialized = true;

                const size_t rowStride = static_cast<size_t>(width);
                exr_coding_channel_info_t *channels = encode.channels;
                for (int c = 0; c < encode.channel_count; ++c)
                {
                    exr_coding_channel_info_t &channel = channels[c];
                    const char *channelName = channel.channel_name;
                    const float *channelData = red.data();

                    if (channelName && std::strcmp(channelName, "R") == 0) channelData = red.data();
                    else if (channelName && std::strcmp(channelName, "G") == 0) channelData = green.data();
                    else if (channelName && std::strcmp(channelName, "B") == 0) channelData = blue.data();
                    else if (channelName && std::strcmp(channelName, "A") == 0) channelData = alpha.data();

                    channel.user_data_type = EXR_PIXEL_FLOAT;
                    channel.user_bytes_per_element = sizeof(float);
                    channel.user_pixel_stride = sizeof(float);
                    channel.user_line_stride = static_cast<int32_t>(sizeof(float) * rowStride);
                    channel.encode_from_ptr = reinterpret_cast<const uint8_t *>(channelData);
                }

                rv = exr_encoding_choose_default_routines(ctx, partIndex, &encode);
                if (rv != EXR_ERR_SUCCESS)
                {
                    break;
                }

                for (int y = 0; y < height; ++y)
                {
                    rv = exr_write_scanline_chunk_info(ctx, partIndex, y, &chunk);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        break;
                    }

                    rv = exr_encoding_update(ctx, partIndex, &chunk, &encode);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        break;
                    }

                    rv = exr_encoding_run(ctx, partIndex, &encode);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        break;
                    }
                }

                success = (rv == EXR_ERR_SUCCESS);
            } while (false);

            if (encodeInitialized)
            {
                exr_encoding_destroy(ctx, &encode);
            }

            exr_finish(&ctx);
            return success;
        }

        static std::vector<std::byte> SerializeMaterial(const Ref<Material> &mat, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            // Write name
            uint32_t strSize = 0;
            AppendString(buffer, mat->name, strSize);

            MaterialType matType = mat->GetType();
            AppendRaw(buffer, matType);

            AppendRaw(buffer, mat->baseColorTextureHandle);
            AppendRaw(buffer, mat->emissiveTextureHandle);
            AppendRaw(buffer, mat->metallicTextureHandle);
            AppendRaw(buffer, mat->roughnessTextureHandle);
            AppendRaw(buffer, mat->normalTextureHandle);
            AppendRaw(buffer, mat->occlusionTextureHandle);

            AppendRaw(buffer, mat->gpuData);

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

			ReadRaw(inFile, &mat->baseColorTextureHandle);
			ReadRaw(inFile, &mat->emissiveTextureHandle);
            ReadRaw(inFile, &mat->metallicTextureHandle);
            ReadRaw(inFile, &mat->roughnessTextureHandle);
			ReadRaw(inFile, &mat->normalTextureHandle);
            ReadRaw(inFile, &mat->occlusionTextureHandle);

            ReadRaw(inFile, &mat->gpuData);

            inFile.close();
            return mat;
        }

        static std::vector<std::byte> SerializeMesh(const Mesh *mesh, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            const std::vector<Ref<MeshInstance>> &meshInstances = mesh->GetMeshInstances();
            uint32_t meshCount = static_cast<uint32_t>(meshInstances.size());
            AppendRaw(buffer, meshCount);

            for (auto &m : meshInstances)
            {
                auto &primitive = m->GetPrimitive();

                uint32_t verticesCount = static_cast<uint32_t>(primitive->vertices.size());
                uint32_t indicesCount = static_cast<uint32_t>(primitive->indices.size());
                AppendRaw(buffer, verticesCount);
                AppendRaw(buffer, indicesCount);

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

                AppendBytes(buffer, primitive->indices.data(), indicesCount * sizeof(uint32_t));

                uint32_t nameSize = 0;
                AppendString(buffer, m->GetName(), nameSize);

                for (int i = 0; i < 4; ++i)
                {
                    AppendRaw(buffer, m->local[i].x);
                    AppendRaw(buffer, m->local[i].y);
                    AppendRaw(buffer, m->local[i].z);
                    AppendRaw(buffer, m->local[i].w);
                }

                for (int i = 0; i < 4; ++i)
                {
                    AppendRaw(buffer, m->global[i].x);
                    AppendRaw(buffer, m->global[i].y);
                    AppendRaw(buffer, m->global[i].z);
                    AppendRaw(buffer, m->global[i].w);
                }

                AppendRaw(buffer, m->linkedJointIndex);

                uint64_t materialHandle = m->GetMaterialHandle();
                AppendRaw(buffer, materialHandle);
            }

            const uint64_t skeletonHandle = static_cast<uint64_t>(mesh->GetSkeletonHandle());
            AppendRaw(buffer, skeletonHandle);

            const uint64_t animatorHandle = static_cast<uint64_t>(mesh->GetAnimatorHandle());
            AppendRaw(buffer, animatorHandle);

            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<Mesh> DeserializeMesh(const std::filesystem::path &filepath)
        {
            Ref<Mesh> skeletalMesh = Mesh::Create();

            std::ifstream inFile(filepath, std::ios::binary);
            if (!inFile)
            {
                return nullptr;
            }

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

                primitive->indices.resize(indicesCount);
                ReadRaw(inFile, primitive->indices.data(), indicesCount * sizeof(uint32_t));
                primitive->RecalculateAABB();

                uint32_t nameSize = 0;
                ReadRaw(inFile, &nameSize);
                name = ReadString(inFile, nameSize);

                for (int j = 0; j < 4; ++j)
                {
                    ReadRaw(inFile, &meshInstance->local[j].x);
                    ReadRaw(inFile, &meshInstance->local[j].y);
                    ReadRaw(inFile, &meshInstance->local[j].z);
                    ReadRaw(inFile, &meshInstance->local[j].w);
                }

                for (int j = 0; j < 4; ++j)
                {
                    ReadRaw(inFile, &meshInstance->global[j].x);
                    ReadRaw(inFile, &meshInstance->global[j].y);
                    ReadRaw(inFile, &meshInstance->global[j].z);
                    ReadRaw(inFile, &meshInstance->global[j].w);
                }

                ReadRaw(inFile, &meshInstance->linkedJointIndex);

                uint64_t materialHandle = 0;
                ReadRaw(inFile, &materialHandle);
                if (materialHandle != 0)
                {
                    meshInstance->SetMaterial(AssetHandle(materialHandle));
                }

                skeletalMesh->AddMeshInstance(meshInstance);
            }

            uint64_t skeletonHandle = 0;
            if (ReadRaw(inFile, &skeletonHandle) && skeletonHandle != 0)
            {
                skeletalMesh->SetSkeleton(AssetHandle(skeletonHandle));
            }

            uint64_t animatorHandle = 0;
            if (ReadRaw(inFile, &animatorHandle) && animatorHandle != 0)
            {
                skeletalMesh->SetAnimator(AssetHandle(animatorHandle));
            }

            inFile.close();
            return skeletalMesh;
        }

        static std::vector<std::byte> SerializeSkeletalAnimation(SkeletalAnimation *anim, const std::filesystem::path &filepath)
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

            AppendRaw(buffer, (uint64_t)anim->GetSkeletonHandle());

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<SkeletalAnimation> DeserializeSkeletalAnimation(const std::filesystem::path &filepath)
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

            uint64_t skeletonHandle = 0;
            if (ReadRaw(inFile, &skeletonHandle) && skeletonHandle != 0)
            {
                anim->SetSkeletonHandle(UUID(skeletonHandle));
            }

            inFile.close();

            return anim;
        }

        static std::vector<std::byte> SerializeSkeleton(Skeleton *skeleton, const std::filesystem::path &filepath)
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

            for (const JointSocket &socket : skeleton->sockets)
            {
                if (!nameOffsets.contains(socket.name))
                {
                    uint32_t offset = static_cast<uint32_t>(stringTable.size());
                    nameOffsets[socket.name] = offset;
                    stringTable += socket.name;
                    stringTable += '\0';
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

            // sockets
            uint32_t socketCount = static_cast<uint32_t>(skeleton->sockets.size());
            AppendRaw(buffer, socketCount);
            for (const JointSocket &socket : skeleton->sockets)
            {
                DiskSocket ds{};
                ds.nameOffset = nameOffsets[socket.name];
                ds.parentId = socket.parentJointId;

                ds.translation[0] = socket.localTranslation.x;
                ds.translation[1] = socket.localTranslation.y;
                ds.translation[2] = socket.localTranslation.z;

                ds.rotation[0] = socket.localRotation.x;
                ds.rotation[1] = socket.localRotation.y;
                ds.rotation[2] = socket.localRotation.z;
                ds.rotation[3] = socket.localRotation.w;

                ds.scale[0] = socket.localScale.x;
                ds.scale[1] = socket.localScale.y;
                ds.scale[2] = socket.localScale.z;

                AppendRaw(buffer, ds);
            }

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

            uint32_t socketCount = 0;
            ReadRaw(inFile, &socketCount);

            std::vector<DiskSocket> diskSockets(socketCount);
            if (socketCount > 0)
            {
                ReadRaw(inFile, diskSockets.data(), sizeof(DiskSocket) * socketCount);
            }

            skeleton->sockets.reserve(socketCount);
            for (const DiskSocket &ds : diskSockets)
            {
                if (ds.nameOffset >= stringTableSize)
                {
                    continue;
                }

                const char *socketNamePtr = stringTable.data() + ds.nameOffset;
                JointSocket socket{};
                socket.name = std::string(socketNamePtr);
                socket.parentJointId = ds.parentId;
                socket.localTranslation = glm::vec3(ds.translation[0], ds.translation[1], ds.translation[2]);
                socket.localRotation = glm::quat(ds.rotation[3], ds.rotation[0], ds.rotation[1], ds.rotation[2]);
                socket.localScale = glm::vec3(ds.scale[0], ds.scale[1], ds.scale[2]);

                skeleton->socketNameToIndex[socket.name] = static_cast<int32_t>(skeleton->sockets.size());
                skeleton->sockets.push_back(std::move(socket));
            }

            inFile.close();

            return skeleton;
        }
    };
}
