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

#include "mesh_loader.hpp"

#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"

#include "renderer.hpp"
#include "lighting.hpp"
#include "ignite/math/math.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"
#include "ignite/graphics/environment.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/animation/skeleton.hpp"

#include <queue>
#include <stb_image.h>

namespace ignite
{
    // Mesh loader
    void MeshLoader::ProcessNode(const aiScene *scene, const aiNode *node, const std::filesystem::path &filepath,
        std::vector<Ref<Mesh>> &meshes, std::vector<NodeInfo> &nodes, const Ref<Skeleton> &skeleton, const int parentNodeID)
    {
        // Create a node entry and get its index
        NodeInfo nodeInfo;
        const int currentNodeID = static_cast<int>(nodes.size());

        nodeInfo.localTransform = Math::AssimpToGlmMatrix(node->mTransformation);
        nodeInfo.id = currentNodeID;
        nodeInfo.parentID = parentNodeID;
        nodeInfo.name = node->mName.C_Str();

        nodes.push_back(nodeInfo);

        // If parent exists, add this node as a child
        if (parentNodeID != -1)
        {
            nodes[parentNodeID].childrenIDs.push_back(currentNodeID);
        }

        // Load meshes
        for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            int meshIndex = node->mMeshes[i];
            aiMesh *assimpMesh = scene->mMeshes[meshIndex];
            const Ref<Mesh> &mesh = meshes[meshIndex];

            mesh->data.name = assimpMesh->mName.C_Str();
            mesh->data.meshIndex = meshIndex;
            mesh->data.materialIndex = assimpMesh->mMaterialIndex;

            // Set node
            mesh->data.nodeID = currentNodeID;

            // Set parent node
            if (nodeInfo.parentID != -1)
            {
                // Go up 
                const NodeInfo &parentNode = nodes[nodeInfo.parentID];
                if (skeleton)
                {
                    if (const auto it = skeleton->nameToJointMap.find(parentNode.name); it != skeleton->nameToJointMap.end())
                    {
                        mesh->data.nodeParentID = nodeInfo.parentID;
                    }
                }
            }

            // Store mesh index in the node
            nodes[currentNodeID].meshIndices.push_back(meshIndex);

            LoadSingleMesh(assimpMesh, mesh->data, mesh->aabb);

            // Load bones
            if (skeleton != nullptr && assimpMesh->HasBones())
            {
                ProcessBoneWeights(assimpMesh, meshIndex, mesh->data, skeleton);
            }

            LOG_WARN("[Mesh Loader] {} [{}] Loaded", assimpMesh->mName.data, meshIndex);
        }

        // Process all children with this node as parent
        for (u32 i = 0; i < node->mNumChildren; ++i)
        {
            ProcessNode(scene, node->mChildren[i], filepath, meshes, nodes, skeleton, currentNodeID);
        }
    }

    void MeshLoader::LoadSingleMesh(aiMesh *mesh, MeshData &outMeshData, AABB &outAABB)
    {
        // vertices;
        VertexMesh_Anim vertex;
        outMeshData.vertices.resize(mesh->mNumVertices);

        outAABB.min = glm::vec3(FLT_MAX);
        outAABB.max = glm::vec3(-FLT_MAX);

        for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
        {
            vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

            outAABB.min = glm::min(outAABB.min, vertex.position);
            outAABB.max = glm::max(outAABB.max, vertex.position);

            if (mesh->HasNormals())
                vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            else 
                vertex.normal = { 0.0f, 1.0f, 0.0f }; // default normals

            if (mesh->mTextureCoords[0])
                vertex.texCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            else
                vertex.texCoord = { 0.0f, 0.0f };

            // Initialize boneIDs and weights to default values
            for (int j = 0; j < VERTEX_MAX_BONES; ++j)
            {
                vertex.boneIDs[j] = 0;
                vertex.weights[j] = 0.0f;
            }

            outMeshData.vertices[i] = vertex;
        }

        outMeshData.indices.reserve(mesh->mNumFaces * 3);
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            aiFace face = mesh->mFaces[i];
            outMeshData.indices.push_back(face.mIndices[0]);
            outMeshData.indices.push_back(face.mIndices[1]);
            outMeshData.indices.push_back(face.mIndices[2]);
        }
    }

    void MeshLoader::ProcessBoneWeights(const aiMesh *assimpMesh, int meshIndex, MeshData &outMeshData, const Ref<Skeleton> &skeleton)
    {
        skeleton->boneMapping[meshIndex].boneMapping.clear();
        skeleton->boneMapping[meshIndex].boneInfo.resize(skeleton->joints.size());

        // Copy bone offset from skeleton
        for (size_t i = 0; i < skeleton->joints.size(); ++i)
        {
            skeleton->boneMapping[meshIndex].boneInfo[i].offsetMatrix = skeleton->joints[i].inverseBindPose;
            skeleton->boneMapping[meshIndex].boneMapping[skeleton->joints[i].name] = static_cast<int>(i);
        }

        for (uint32_t boneIndex = 0; boneIndex < assimpMesh->mNumBones; ++boneIndex)
        {
            aiBone *bone = assimpMesh->mBones[boneIndex];
            std::string boneName = bone->mName.C_Str();

            // Get bone ID from skeleton
            auto it = skeleton->nameToJointMap.find(boneName);
            if (it == skeleton->nameToJointMap.end())
            {
                LOG_WARN("[Model Loader]: Bone {} not found in skeleton!", boneName);
                continue;
            }

            uint32_t boneId = it->second;

            // Each vertex can be affected by multiple bones
            for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
            {
                uint32_t vertexId = bone->mWeights[weightIndex].mVertexId;
                float weight = bone->mWeights[weightIndex].mWeight;

                // Find the first empty slot in this vertex's bone array
                const float EPSILON = 0.00001f;
                for (uint32_t j = 0; j < VERTEX_MAX_BONES; ++j)
                {
                    if (outMeshData.vertices[vertexId].weights[j] < EPSILON)
                    {
                        outMeshData.vertices[vertexId].boneIDs[j] = boneId;
                        outMeshData.vertices[vertexId].weights[j] = weight;
                        break;
                    }
                }
            }
        }

        // Normalize weights to ensure the sum to 1.0
        for (auto &vertex : outMeshData.vertices)
        {
            float totalWeight = 0.0f;
            for (const float &weight : vertex.weights)
            {
                totalWeight += weight;
            }

            if (totalWeight > 0.0f)
            {
                for (float& weight : vertex.weights)
                {
                    weight /= totalWeight;
                }
            }
        }
    }

    void MeshLoader::ExtractSkeleton(const aiScene *scene, Ref<Skeleton> &skeleton)
    {
        // count the number of joints
        std::unordered_set<std::string> uniqueJointNames;
        for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
        {
            aiMesh *mesh = scene->mMeshes[m];
            for (uint32_t b = 0; b < mesh->mNumBones; ++b)
            {
                uniqueJointNames.insert(mesh->mBones[b]->mName.C_Str());
            }
        }

        skeleton->joints.reserve(uniqueJointNames.size());
        // create joints map and collect inverse bind matrices
        std::unordered_map<std::string, glm::mat4> inverseBindMatrices;
        for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
        {
            aiMesh *mesh = scene->mMeshes[m];
            for (uint32_t b = 0; b < mesh->mNumBones; ++b)
            {
                aiBone *bone = mesh->mBones[b];
                std::string boneName = bone->mName.C_Str();
                inverseBindMatrices[boneName] = Math::AssimpToGlmMatrix(bone->mOffsetMatrix);
            }
        }

        // Find all nodes related to the skeleton
        ExtractSkeletonRecursive(scene->mRootNode, -1, skeleton, inverseBindMatrices);
    }

    void MeshLoader::ExtractSkeletonRecursive(const aiNode *node, int parentJointId, Ref<Skeleton> &skeleton, const std::unordered_map<std::string, glm::mat4> &inverseBindMatrices)
    {
        std::string nodeName = node->mName.C_Str();
        bool isJoint = inverseBindMatrices.contains(nodeName);
        int currentJointId = -1;
        if (isJoint)
        {
            // Add this node as a joint
            Joint joint;
            joint.name = nodeName;
            joint.id = static_cast<int>(skeleton->joints.size());
            joint.parentJointId = parentJointId;
            joint.inverseBindPose = inverseBindMatrices.at(nodeName);
            joint.localTransform = Math::AssimpToGlmMatrix(node->mTransformation);

            currentJointId = joint.id;
            skeleton->nameToJointMap[nodeName] = currentJointId;
            skeleton->joints.push_back(joint);
        }

        // process child (use parent id if this node is not a joint)
        int childParentId = isJoint ? currentJointId : parentJointId;
        for (uint32_t i = 0; i < node->mNumChildren; ++i)
        {
            ExtractSkeletonRecursive(node->mChildren[i], childParentId, skeleton, inverseBindMatrices);
        }
    }

    void MeshLoader::SortJointsHierarchically(Ref<Skeleton> &skeleton)
    {
        std::vector<Joint> sortedJoints;
        sortedJoints.reserve(skeleton->joints.size());
        // use a queue to process joints level by level
        std::queue<int> queue;
        // start with root joints
        for (size_t i = 0; i < skeleton->joints.size(); ++i)
        {
            if (skeleton->joints[i].parentJointId == -1)
                queue.push(i);
        }
        // BFS traversal to ensure parents are processed before children
        while (!queue.empty())
        {
            int jointIdx = queue.front();
            queue.pop();
            sortedJoints.push_back(skeleton->joints[jointIdx]);
            int newIdx = static_cast<int>(sortedJoints.size()) - 1;
            // Update joint indices in the new array
            if (sortedJoints[newIdx].parentJointId != -1)
            {
                // Find new parent index
                std::string parentName = skeleton->joints[sortedJoints[newIdx].parentJointId].name;
                for (int j = 0; j < newIdx; ++j)
                {
                    if (sortedJoints[j].name == parentName)
                    {
                        sortedJoints[newIdx].parentJointId = j;
                        break;
                    }
                }
            }
            // Add children to queue
            for (size_t i = 0; i < skeleton->joints.size(); ++i)
            {
                if (skeleton->joints[i].parentJointId == jointIdx)
                {
                    queue.push(i);
                }
            }
        }
        // Update name to joint name
        skeleton->nameToJointMap.clear();
        for (size_t i = 0; i < sortedJoints.size(); ++i)
        {
            sortedJoints[i].id = i;
            skeleton->nameToJointMap[sortedJoints[i].name] = i;
        }
        skeleton->joints = std::move(sortedJoints);
    }

    void MeshLoader::LoadAnimation(const aiScene *scene, std::vector<SkeletalAnimation> &animations)
    {
        animations.resize(scene->mNumAnimations);

        for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
        {
            aiAnimation *anim = scene->mAnimations[i];
            animations[i] = SkeletalAnimation(anim);
        }
    }

    void MeshLoader::CalculateWorldTransforms(std::vector<NodeInfo> &nodes)
    {
        // First pass: calculate world transforms for nodes
        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (nodes[i].parentID == -1)
            {
                // Root node
                nodes[i].worldTransform = nodes[i].localTransform;
            }
            else
            {
                // Child node
                nodes[i].worldTransform = nodes[nodes[i].parentID].worldTransform * nodes[i].localTransform;
            }
#if 0
            // Apply node's world transform to all its meshes
            for (int meshIdx : nodes[i].meshIndices)
            {
                outMeshIndexGlobalMatrices[meshIdx] = nodes[i].worldTransform;
            }
#endif
        }
    }
}
