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

#include "stb_image_write.h"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"

#include "ignite/graphics/mesh.hpp"

#include <filesystem>
#include <vector>
#include <cinttypes>

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

            // write name
            std::string nameCopy = mat->name;
            nameCopy += '\0';
            uint32_t nameSize = static_cast<uint32_t>(nameCopy.size());
            AppendRaw(buffer, nameSize);

            buffer.insert(buffer.end(),
                reinterpret_cast<const std::byte *>(nameCopy.data()),
                reinterpret_cast<const std::byte *>(nameCopy.data()) + nameSize
            );

            AppendRaw(buffer, mat->params);
            AppendRaw(buffer, mat->type);
            AppendRaw(buffer, mat->blendMode);
            AppendRaw(buffer, mat->mipLevels);

            // write texture
            uint32_t textureCount = static_cast<uint32_t>(mat->textures.size());
            AppendRaw(buffer, textureCount);

            for (auto &[type, tex] : mat->textures)
            {
                // write texture type
                AppendRaw(buffer, type);

                AppendRaw(buffer, tex->width);
                AppendRaw(buffer, tex->height);
                AppendRaw(buffer, tex->rowPitch);

                const std::size_t pixelBytes = static_cast<std::size_t>(tex->height) * tex->rowPitch;

                // write RGBA blob
                const std::byte *begin = reinterpret_cast<const std::byte *>(tex->data);
                buffer.insert(buffer.end(), begin, begin + pixelBytes);
            }

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
                throw std::runtime_error("Cannot open material file " + filepath.string());
            }

            // read name
            uint32_t nameSize = 0;
            inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
            std::vector<char> stringBytes(nameSize); // owns the bytes
            inFile.read(stringBytes.data(), nameSize);
            mat->name = std::string(stringBytes.data());

            inFile.read(reinterpret_cast<char *>(&mat->params), sizeof(mat->params));
            inFile.read(reinterpret_cast<char *>(&mat->type), sizeof(mat->type));
            inFile.read(reinterpret_cast<char *>(&mat->blendMode), sizeof(mat->blendMode));
            inFile.read(reinterpret_cast<char *>(&mat->mipLevels), sizeof(mat->mipLevels));

            // write texture
            uint32_t textureCount = 0;
            inFile.read(reinterpret_cast<char *>(&textureCount), sizeof(textureCount));

            for (uint32_t i = 0; i < textureCount; ++i)
            {
                Ref<MaterialTextureResource> tex = CreateRef<MaterialTextureResource>();

                MaterialTextureType textureType;
                inFile.read(reinterpret_cast<char *>(&textureType), sizeof(textureType));

                inFile.read(reinterpret_cast<char *>(&tex->width), sizeof(tex->width));
                inFile.read(reinterpret_cast<char *>(&tex->height), sizeof(tex->height));
                inFile.read(reinterpret_cast<char *>(&tex->rowPitch), sizeof(tex->rowPitch));

                const std::size_t pixelBytes = static_cast<std::size_t>(tex->height) * tex->rowPitch;
                if (pixelBytes > 0)
                {
                    tex->data = new uint8_t[pixelBytes];
                }
                
                // read blob *into* the buffer, not into the pointer itself
                inFile.read(reinterpret_cast<char *>(tex->data), pixelBytes);
                mat->textures[textureType] = tex;
            }

            inFile.close();

            return mat;
        }

        static std::vector<std::byte> SerializeMeshAsset(const Ref<MeshAsset> &sm, const std::filesystem::path &filepath)
        {
            std::vector<std::byte> buffer;

            // write nodes vector
            uint32_t nodeCount = static_cast<uint32_t>(sm->nodes.size());
            AppendRaw(buffer, nodeCount);

            uint32_t nodeSizeInBytes = nodeCount * sizeof(NodeInfo);
            AppendRaw(buffer, nodeSizeInBytes);

            // write mesh vector info
            uint32_t meshCount = static_cast<uint32_t>(sm->meshesData.size());
            AppendRaw(buffer, meshCount);

            uint32_t meshesSizeInBytes = meshCount * sizeof(MeshData);
            AppendRaw(buffer, meshesSizeInBytes);

            for (const auto &node : sm->nodes)
            {
                AppendRaw(buffer, node.id);            // id
                AppendRaw(buffer, node.parentID);      // parentID
                AppendRaw(buffer, node.materialIndex); // material index
                AppendRaw(buffer, node.isJoint);       // joint flag

                // write children vector
                uint32_t childrenIDCount = static_cast<uint32_t>(node.childrenIDs.size());
                AppendRaw(buffer, childrenIDCount);

                // write referenced mesh indices
                uint32_t meshIndicesCount = static_cast<uint32_t>(node.meshIndices.size());
                AppendRaw(buffer, meshIndicesCount);
                
                // write name
                std::string nameCopy = node.name;
                nameCopy += '\0';
                // name size in bytes including null-terminated
                uint32_t nameSize = static_cast<uint32_t>(nameCopy.size());
                AppendRaw(buffer, nameSize);

                buffer.insert(buffer.end(),
                    reinterpret_cast<const std::byte *>(nameCopy.data()),
                    reinterpret_cast<const std::byte *>(nameCopy.data() + nameSize)
                );

                // write local transform
                for (int i = 0; i < 4; ++i)
                {
                    AppendRaw(buffer, node.localTransform[i].x);
                    AppendRaw(buffer, node.localTransform[i].y);
                    AppendRaw(buffer, node.localTransform[i].z);
                    AppendRaw(buffer, node.localTransform[i].w);
                }

                // ---------------------------
                // SKIPPED WORLD TRANSFORM
                // ---------------------------

                // write children vector
                for (int id : node.childrenIDs)
                {
                    AppendRaw(buffer, id);
                }

                // write referenced mesh indices
                for (int index : node.meshIndices)
                {
                    AppendRaw(buffer, index);
                }
            }

            // write mesh vector

            for (auto &mesh : sm->meshesData)
            {
                AppendRaw(buffer, mesh.meshIndex);
                AppendRaw(buffer, mesh.materialIndex);
                AppendRaw(buffer, mesh.nodeParentID);
                AppendRaw(buffer, mesh.nodeID);

                // write vertex data
                uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());
                AppendRaw(buffer, vertexCount);

                // write indices data
                uint32_t indicesCount = static_cast<uint32_t>(mesh.indices.size());
                AppendRaw(buffer, indicesCount);

                // write name
                std::string nameCopy = mesh.name;
                nameCopy += '\0';
                // name size in bytes including null-terminated
                uint32_t nameSize = static_cast<uint32_t>(nameCopy.size());
                AppendRaw(buffer, nameSize);

                buffer.insert(buffer.end(),
                    reinterpret_cast<const std::byte *>(nameCopy.data()),
                    reinterpret_cast<const std::byte *>(nameCopy.data() + nameSize)
                );

                // write vertices
                for (auto &vertex : mesh.vertices)
                {
                    AppendRaw(buffer, vertex.position);
                    AppendRaw(buffer, vertex.normal);
                    AppendRaw(buffer, vertex.texCoord);
                    AppendRaw(buffer, vertex.color);

                    AppendRaw(buffer, vertex.boneIDs);
                    AppendRaw(buffer, vertex.weights);

                    // ---------------------------
                    // SKIPPED ENTITY ID
                    // ---------------------------
                }

                // write indices
                buffer.insert(buffer.end(),
                    reinterpret_cast<const std::byte *>(mesh.indices.data()),
                    reinterpret_cast<const std::byte *>(mesh.indices.data()) + indicesCount * sizeof(uint32_t)
                );
            }

            // Write to file
            std::ofstream of(filepath, std::ios::binary);
            of.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
            of.close();

            return buffer;
        }

        static Ref<MeshAsset> DeserializeMeshAsset(const std::filesystem::path &filepath)
        {
            Ref<MeshAsset> meshAsset = CreateRef<MeshAsset>();

            std::ifstream inFile(filepath, std::ios::binary);

            if (!inFile)
            {
                throw std::runtime_error("Cannot open skeletal mesh file " + filepath.string());
            }

            // write nodes vector
            uint32_t nodeCount = 0;
            inFile.read(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));

            uint32_t nodeSizeInBytes = 0;
            inFile.read(reinterpret_cast<char *>(&nodeSizeInBytes), sizeof(nodeSizeInBytes));

            // write mesh vector
            uint32_t meshCount = 0;
            inFile.read(reinterpret_cast<char *>(&meshCount), sizeof(meshCount));

            uint32_t meshesSizeInBytes = 0;
            inFile.read(reinterpret_cast<char *>(&meshesSizeInBytes), sizeof(meshesSizeInBytes));

            meshAsset->nodes.reserve(nodeCount);
            for (uint32_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
            {
                NodeInfo node;

                inFile.read(reinterpret_cast<char *>(&node.id), sizeof(node.id));
                inFile.read(reinterpret_cast<char *>(&node.parentID), sizeof(node.parentID));
                inFile.read(reinterpret_cast<char *>(&node.materialIndex), sizeof(node.materialIndex));
                inFile.read(reinterpret_cast<char *>(&node.isJoint), sizeof(node.isJoint));

                // read children count
                uint32_t childrenIDCount = 0;
                inFile.read(reinterpret_cast<char *>(&childrenIDCount), sizeof(childrenIDCount));

                // read mesh count
                uint32_t meshIndicesCount = 0;
                inFile.read(reinterpret_cast<char *>(&meshIndicesCount), sizeof(meshIndicesCount));

                // read name
                uint32_t nameSize = 0;
                inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
                std::vector<char> stringBytes(nameSize); // owns the bytes
                inFile.read(stringBytes.data(), nameSize);
                node.name = std::string(stringBytes.data());

                // read local transform
                for (int i = 0; i < 4; ++i)
                {
                    inFile.read(reinterpret_cast<char *>(&node.localTransform[i].x), sizeof(float));
                    inFile.read(reinterpret_cast<char *>(&node.localTransform[i].y), sizeof(float));
                    inFile.read(reinterpret_cast<char *>(&node.localTransform[i].z), sizeof(float));
                    inFile.read(reinterpret_cast<char *>(&node.localTransform[i].w), sizeof(float));
                }

                // read children ids
                node.childrenIDs.reserve(childrenIDCount);
                for (uint32_t childIndex = 0; childIndex < childrenIDCount; ++childIndex)
                {
                    int index = -1;
                    inFile.read(reinterpret_cast<char *>(&index), sizeof(int));
                    node.childrenIDs.push_back(index);
                }

                // read mesh indices
                for (uint32_t meshIndex = 0; meshIndex < meshIndicesCount; ++meshIndex)
                {
                    int index = -1;
                    inFile.read(reinterpret_cast<char *>(&index), sizeof(index));
                    node.meshIndices.push_back(index);
                }

                meshAsset->nodes.push_back(node);
            }

            // read mesh vector
            meshAsset->meshesData.reserve(meshCount);
            for (uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex)
            {
                MeshData mesh;
                mesh.aabb.min = glm::vec3(FLT_MAX);
                mesh.aabb.max = glm::vec3(-FLT_MAX);

                inFile.read(reinterpret_cast<char *>(&mesh.meshIndex), sizeof(mesh.meshIndex));
                inFile.read(reinterpret_cast<char *>(&mesh.materialIndex), sizeof(mesh.materialIndex));
                inFile.read(reinterpret_cast<char *>(&mesh.nodeParentID), sizeof(mesh.nodeParentID));
                inFile.read(reinterpret_cast<char *>(&mesh.nodeID), sizeof(mesh.nodeID));

                uint32_t vertexCount = 0;
                inFile.read(reinterpret_cast<char *>(&vertexCount), sizeof(vertexCount));

                uint32_t indexCount = 0;
                inFile.read(reinterpret_cast<char *>(&indexCount), sizeof(indexCount));

                // read name
                uint32_t nameSize = 0;
                inFile.read(reinterpret_cast<char *>(&nameSize), sizeof(nameSize));
                std::vector<char> stringBytes(nameSize); // owns the bytes
                inFile.read(stringBytes.data(), nameSize);
                mesh.name = std::string(stringBytes.data());

                // read vertices
                mesh.vertices.reserve(vertexCount);
                for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
                {
                    VertexMesh_Anim vertex;
                    inFile.read(reinterpret_cast<char *>(&vertex.position), sizeof(vertex.position));
                    inFile.read(reinterpret_cast<char *>(&vertex.normal), sizeof(vertex.normal));
                    inFile.read(reinterpret_cast<char *>(&vertex.texCoord), sizeof(vertex.texCoord));
                    inFile.read(reinterpret_cast<char *>(&vertex.color), sizeof(vertex.color));
                    inFile.read(reinterpret_cast<char *>(vertex.boneIDs), sizeof(vertex.boneIDs));
                    inFile.read(reinterpret_cast<char *>(vertex.weights), sizeof(vertex.weights));

                    // load aabb
                    mesh.aabb.min = glm::min(mesh.aabb.min, vertex.position);
                    mesh.aabb.max = glm::max(mesh.aabb.max, vertex.position);

                    mesh.vertices.push_back(vertex);
                }

                // read indices
                mesh.indices.resize(indexCount);
                inFile.read(reinterpret_cast<char *>(mesh.indices.data()), indexCount * sizeof(uint32_t));

                meshAsset->meshesData.push_back(mesh);
            }

            inFile.close();
            
            return meshAsset;
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
