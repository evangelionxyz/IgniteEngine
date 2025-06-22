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

#include "mesh.hpp"
#include "ignite/animation/skeletal_animation.hpp"

namespace ignite
{
    class Skeleton;

    class MeshLoader
    {
    public:        
        static void ProcessNode(const aiScene *scene, const aiNode *node, const std::filesystem::path &filepath, std::vector<Ref<Mesh>> &meshes, std::vector<NodeInfo> &nodes, const Ref<Skeleton> &skeleton, int parentNodeID);
        static void LoadSingleMesh(aiMesh *mesh, MeshData &outMeshData, AABB &outAABB);
        static void ProcessBoneWeights(const aiMesh *assimpMesh, int meshIndex, MeshData &outMeshData, const Ref<Skeleton> &skeleton);

        static void ExtractSkeleton(const aiScene *scene, Ref<Skeleton> &skeleton);
        static void ExtractSkeletonRecursive(const aiNode *node, int parentJointId, Ref<Skeleton> &skeleton, const std::unordered_map<std::string, glm::mat4> &inverseBindMatrices);
        static void SortJointsHierarchically(Ref<Skeleton> &skeleton);
        static void LoadAnimation(const aiScene *scene, std::vector<SkeletalAnimation> &animations);
        static void CalculateWorldTransforms(std::vector<NodeInfo> &nodes);
    };    
}
