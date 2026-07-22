// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BINARY_SERIALIZER_HPP
#define IGN_BINARY_SERIALIZER_HPP

#include <stb_image.h>
#include <stb_image_write.h>
#include <openexr.h>
#include <openexr_errors.h>

#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/objects/mesh.hpp"

#include "ignite/core/path.hpp"
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
                return !stream.fail();
            }
            return false;
        }

        static std::string ReadString(std::ifstream &stream, uint32_t strSize)
        {
            if (strSize == 0)
                return "";

            // Limit to maximum sensible string size (e.g 1MB) to prevent OOM
            if (strSize > 1024 * 1024)
            {
                throw std::runtime_error("String size exceeds safety limit");
            }

            if (!HasRemainingBytes(stream, strSize))
            {
                throw std::runtime_error("Corrupt file: string size exceeds remaining file size");
            }


            std::vector<char> stringBytes(strSize); // owns the bytes
            stream.read(stringBytes.data(), strSize);
            if (stream.gcount() < static_cast<std::streamsize>(strSize))
            {
                throw std::runtime_error("Corrupt file: failed to read complete string");
            }

            size_t len = 0;
			while (len < strSize && stringBytes[len] != '\0')
			{
				len++;
			}
			return std::string(stringBytes.data(), len);
        }

        static bool HasRemainingBytes(std::ifstream &stream, size_t neededBytes)
        {
            std::streampos current = stream.tellg();
            stream.seekg(0, std::ios::end);
            
            std::streampos end = stream.tellg();
            stream.seekg(current);

            return (end - current) >= static_cast<std::streamoff>(neededBytes);
        }

        static bool SerializeTextureToPNG(const Ref<Texture> &texture, const ignite::Path &filepath)
        {
            if (!texture)
                return false;

            const int channels = 4;
            const auto width = static_cast<int>(texture->GetWidth());
            const auto height = static_cast<int>(texture->GetHeight());

            if (width <= 0 || height <= 0)
                return false;

            const size_t bytesPerPixel = channels * sizeof(uint8_t);
            const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * bytesPerPixel;

            const Buffer &texturePixel = texture->GetBuffer();
            Buffer pixelCopy(expectedSize);

            if (!texturePixel.IsEmpty())
            {
                pixelCopy = texturePixel;
            }
            else
            {
                const ignite::Path &sourceFilepath = texture->GetFilepath();
                if (sourceFilepath.empty() || !ignite::Path::exists(sourceFilepath))
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

                pixelCopy = Buffer(sourceData, expectedSize);
                stbi_image_free(sourceData);
            }

            int result = stbi_write_png(filepath.generic_string().c_str(), width, height, channels, pixelCopy.Data(), width * channels);
            return result == 1;
        }

        static bool SerializeTextureToEXR(const Ref<Texture> &texture, const ignite::Path &filepath)
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
            if (buffer.IsEmpty())
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
                if (buffer.Size() < pixelCount * 4u * sizeof(float))
                {
                    return false;
                }

                const auto src = (const float *)buffer.Data();
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
                if (buffer.Size() < pixelCount * 4u)
                {
                    return false;
                }

                const uint8_t *src = buffer.Data();
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
            else if (buffer.Size() >= pixelCount * 4u * sizeof(float))
            {
                bufferDecoded = fillFromFloatRGBA();
            }
            else if (buffer.Size() >= pixelCount * 4u)
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

        static std::vector<std::byte> SerializeMaterial(const Ref<Material> &mat, const ignite::Path &filepath)
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

        static Ref<Material> DeserializeMaterial(const ignite::Path &filepath)
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

        template<typename MeshType_T, MeshVertex VertexType_T>
	    requires std::is_base_of<Mesh, MeshType_T>::value
        static std::vector<std::byte> SerializeMesh(const MeshType_T *mesh, const ignite::Path &filepath)
        {
            std::vector<std::byte> buffer;

            const auto &meshInstances = mesh->GetMeshInstances();
            uint32_t meshCount = static_cast<uint32_t>(meshInstances.size());
            AppendRaw(buffer, meshCount);

            for (const auto &m : meshInstances)
            {
                const auto &primitive = m->GetPrimitive();

                uint32_t verticesCount = static_cast<uint32_t>(primitive->vertices.size());
                uint32_t indicesCount = static_cast<uint32_t>(primitive->indices.size());
                AppendRaw(buffer, verticesCount);
                AppendRaw(buffer, indicesCount);

                for (const VertexType_T &vertex : primitive->vertices)
                {
                    AppendRaw(buffer, vertex.position);
                    AppendRaw(buffer, vertex.normal);
                    AppendRaw(buffer, vertex.tangent);
                    AppendRaw(buffer, vertex.bitangent);
                    AppendRaw(buffer, vertex.uv);
                    AppendRaw(buffer, vertex.color);

                    if constexpr (SkeletalMeshVertex<VertexType_T>)
					{
						AppendRaw(buffer, vertex.boneIDs);
						AppendRaw(buffer, vertex.weights);
					}
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

                uint64_t materialHandle = m->GetMaterialAssetHandle();
                AppendRaw(buffer, materialHandle);
            }

            if constexpr (SkeletalMeshVertex<VertexType_T>)
			{
				const uint64_t skeletonHandle = static_cast<uint64_t>(mesh->GetSkeletonHandle());
				AppendRaw(buffer, skeletonHandle);

				const uint64_t animatorHandle = static_cast<uint64_t>(mesh->GetAnimatorHandle());
				AppendRaw(buffer, animatorHandle);
			}

            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

		template<typename MeshType_T, MeshVertex VertexType_T>
        requires std::is_base_of<Mesh, MeshType_T>::value
        static Ref<MeshType_T> DeserializeMesh(const ignite::Path &filepath)
        {
            std::ifstream inFile(filepath, std::ios::binary);
            if (!inFile)
            {
                return nullptr;
            }

            Ref<MeshType_T> mesh = MeshType_T::Create();

            uint32_t meshCount = 0;
            ReadRaw(inFile, &meshCount);

            for (uint32_t i = 0; i < meshCount; ++i)
            {
                uint32_t verticesCount = 0, indicesCount = 0;
                ReadRaw(inFile, &verticesCount);
                ReadRaw(inFile, &indicesCount);

                Ref<MeshPrimitive<VertexType_T>> primitive = CreateRef<MeshPrimitive<VertexType_T>>();
                primitive->vertices.reserve(verticesCount);
                for (uint32_t vertexIndex = 0; vertexIndex < verticesCount; ++vertexIndex)
                {
                    VertexType_T vertex;
                    ReadRaw(inFile, &vertex.position);
                    ReadRaw(inFile, &vertex.normal);
                    ReadRaw(inFile, &vertex.tangent);
                    ReadRaw(inFile, &vertex.bitangent);
                    ReadRaw(inFile, &vertex.uv);
                    ReadRaw(inFile, &vertex.color);
					
                    if constexpr (SkeletalMeshVertex<VertexType_T>)
					{
						ReadRaw(inFile, &vertex.boneIDs);
						ReadRaw(inFile, &vertex.weights);
					}

                    primitive->vertices.push_back(vertex);
                }

                primitive->indices.resize(indicesCount);
                ReadRaw(inFile, primitive->indices.data(), indicesCount * sizeof(uint32_t));

                uint32_t nameSize = 0;
                ReadRaw(inFile, &nameSize);
                std::string name = ReadString(inFile, nameSize);

                auto meshInstance = MeshInstanceFor<VertexType_T>::Create(name, primitive);

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

                mesh->AddMeshInstance(meshInstance);
            }

            if constexpr (SkeletalMeshVertex<VertexType_T>)
            {
				uint64_t skeletonHandle = 0;
				if (ReadRaw(inFile, &skeletonHandle) && skeletonHandle != 0)
				{
					mesh->SetSkeleton(AssetHandle(skeletonHandle));
				}

				uint64_t animatorHandle = 0;
				if (ReadRaw(inFile, &animatorHandle) && animatorHandle != 0)
				{
					mesh->SetAnimator(AssetHandle(animatorHandle));
				}
            }

            mesh->CalculateLocalAABB();

            inFile.close();
            return mesh;
        }

        static std::vector<std::byte> SerializeSkeletalAnimation(SkeletalAnimation *anim, const ignite::Path &filepath)
        {
            std::vector<std::byte> buffer;

            // write animation duration and ticks per second
            AppendRaw(buffer, anim->duration);
            AppendRaw(buffer, anim->ticksPerSeconds);

            // write animation name size and name
            uint32_t animationNameSize = 0;
            AppendString(buffer, anim->name, animationNameSize);

            // write channels
            uint32_t channelCount = static_cast<uint32_t>(anim->channels.size());
            AppendRaw(buffer, channelCount);

            for (const auto &[jointIndex, channel] : anim->channels)
            {
                AppendRaw(buffer, jointIndex);

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

            // Optional trailing event chunk. Older readers safely ignore it and
            // older files simply end after the skeleton handle.
            constexpr uint32_t kTimelineEventMagic = 0x544E5645; // "EVNT"
            AppendRaw(buffer, kTimelineEventMagic);
            const uint32_t eventCount = static_cast<uint32_t>(anim->timelineEvents.size());
            AppendRaw(buffer, eventCount);
            for (const AnimationTimelineEvent &event : anim->timelineEvents)
            {
                AppendRaw(buffer, event.normalizedTime);
                const uint8_t action = static_cast<uint8_t>(event.action);
                AppendRaw(buffer, action);
                uint32_t nameSize = 0;
                AppendString(buffer, event.name, nameSize);
                AppendRaw(buffer, static_cast<uint64_t>(event.audioHandle));
                AppendRaw(buffer, static_cast<uint64_t>(event.callbackAsset));
            }

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<SkeletalAnimation> DeserializeSkeletalAnimation(const ignite::Path &filepath)
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

            // read animation name size and the name
            uint32_t animationNameSize = 0;
            ReadRaw(inFile, &animationNameSize);
            anim->name = ReadString(inFile, animationNameSize);

            // read channel count
            uint32_t channelCount = 0;
            ReadRaw(inFile, &channelCount);

            anim->channels.reserve(channelCount);

            for (uint32_t channelIdx = 0; channelIdx < channelCount; ++channelIdx)
            {
                // read joint index
                int jointIndex = -1;
				ReadRaw(inFile, &jointIndex);

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

                anim->channels[jointIndex] = channel;
            }

            uint64_t skeletonHandle = 0;
            if (ReadRaw(inFile, &skeletonHandle) && skeletonHandle != 0)
            {
                anim->SetSkeletonHandle(AssetHandle(skeletonHandle));
            }

            constexpr uint32_t kTimelineEventMagic = 0x544E5645; // "EVNT"
            uint32_t eventMagic = 0;
            if (ReadRaw(inFile, &eventMagic) && eventMagic == kTimelineEventMagic)
            {
                uint32_t eventCount = 0;
                if (!ReadRaw(inFile, &eventCount) || eventCount > 65536)
                    throw std::runtime_error("Corrupt animation timeline event data");
                anim->timelineEvents.reserve(eventCount);
                for (uint32_t i = 0; i < eventCount; ++i)
                {
                    AnimationTimelineEvent event;
                    uint8_t action = 0;
                    uint32_t nameSize = 0;
                    uint64_t audioHandle = 0, callbackHandle = 0;
                    if (!ReadRaw(inFile, &event.normalizedTime) || !ReadRaw(inFile, &action) || !ReadRaw(inFile, &nameSize))
                        throw std::runtime_error("Corrupt animation timeline event entry");
                    event.name = ReadString(inFile, nameSize);
                    if (!ReadRaw(inFile, &audioHandle) || !ReadRaw(inFile, &callbackHandle))
                        throw std::runtime_error("Corrupt animation timeline event handles");
                    event.action = static_cast<AnimationTimelineEvent::Action>(action);
                    event.audioHandle = AssetHandle(audioHandle);
                    event.callbackAsset = AssetHandle(callbackHandle);
                    event.normalizedTime = std::clamp(event.normalizedTime, 0.0f, 1.0f);
                    anim->timelineEvents.push_back(std::move(event));
                }
            }

            inFile.close();

            return anim;
        }

        static std::vector<std::byte> SerializeSkeleton(Skeleton *skeleton, const ignite::Path &filepath)
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

                ds.translation[0] = socket.local.translation.x;
                ds.translation[1] = socket.local.translation.y;
                ds.translation[2] = socket.local.translation.z;

                ds.rotation[0] = socket.local.rotation.x;
                ds.rotation[1] = socket.local.rotation.y;
                ds.rotation[2] = socket.local.rotation.z;
                ds.rotation[3] = socket.local.rotation.w;

                ds.scale[0] = socket.local.scale.x;
                ds.scale[1] = socket.local.scale.y;
                ds.scale[2] = socket.local.scale.z;

                AppendRaw(buffer, ds);
            }

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<Skeleton> DeserializeSkeleton(const ignite::Path &filepath)
        {
            Ref<Skeleton> skeleton = CreateRef<Skeleton>();

            std::ifstream inFile(filepath, std::ios::binary);

            if (!inFile)
            {
                throw std::runtime_error("Cannot open skeleton file " + filepath.string());
            }

            // read joint count
            uint32_t jointCount = 0;
            if (!ReadRaw(inFile, &jointCount) && !HasRemainingBytes(inFile, jointCount * sizeof(DiskJoint)))
            {
                throw std::runtime_error("Corrupt file: invalid joint count");
            }

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
                size_t maxLen = (size_t)(stringTableSize - dj.nameOffset);
                size_t actualLen = 0;
                while (actualLen < maxLen && namePtr[actualLen] != '\0')
                {
                    actualLen++;
                }
                if (actualLen == maxLen)
                {
                    throw std::runtime_error("Corrupt file: string in table is not null-terminated");
                }

                std::string jointName = std::string(namePtr, actualLen);

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
                socket.local.translation = glm::vec3(ds.translation[0], ds.translation[1], ds.translation[2]);
                socket.local.rotation = glm::quat(ds.rotation[3], ds.rotation[0], ds.rotation[1], ds.rotation[2]);
                socket.local.scale = glm::vec3(ds.scale[0], ds.scale[1], ds.scale[2]);

                skeleton->socketNameToIndex[socket.name] = static_cast<int32_t>(skeleton->sockets.size());
                skeleton->sockets.push_back(std::move(socket));
            }

            skeleton->UpdateGlobalTransforms();
            inFile.close();

            return skeleton;
        }
    };
}

#endif
