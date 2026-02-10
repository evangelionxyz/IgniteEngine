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

#include "ipanel.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/texture.hpp"

#include <filesystem>
#include <stack>
#include <map>

namespace ignite
{

    class Project;
    class EditorLayer;

    struct FileTreeNode
    {
        AssetHandle handle = AssetHandle(0);
        std::filesystem::path path;
        // path, index
        std::map<std::filesystem::path, uint32_t> children;
        uint32_t parent = static_cast<uint32_t>(-1);
        bool isDeleted = false;

        FileTreeNode(const std::filesystem::path &path, AssetHandle handle)
            : path(path), handle(handle)
        {
        }
    };

    struct FileThumbnail
    {
        Ref<Texture> thumbnail;
        uint64_t timestamp = 0;
    };

    class ContentBrowserPanel : public IPanel
    {
    public:
        explicit ContentBrowserPanel(const char *windowTitle);
        virtual void OnGuiRender() override;

        virtual void OnUpdate(float deltaTime) override;

        void LoadProjectFiles();

    private:
        void RenderFileTree(FileTreeNode *node);
        void RefreshEntryPathList();
        void RefreshAssetTree();
        void LoadAssetTree(const std::filesystem::path &directory);

        void PruneMissingNodes(uint32_t nodeIndex, const std::filesystem::path &basePath);
        void PruneMissingNodesAlt(uint32_t nodeIndex, const std::filesystem::path &basePath);
        void CollectNodesToDelete(uint32_t nodeIndex, const std::filesystem::path &basePath, std::vector<uint32_t> &nodesToDelete);
        void CollectNodeAndDescendants(uint32_t nodeIndex, std::vector<uint32_t> &nodesToDelete);
        void MarkNodeDeletedRecursive(uint32_t nodeIndex);
        void DeleteSingleNode(uint32_t nodeIndex);
        void UpdateIndicesAfterDeletion(uint32_t deletedIndex);
        void CompactTree();

        void DragDropSource(const std::filesystem::path &filepath);

        static void OnImportAssetDialog(void *userData, const char * const *fileList, int filter);

        std::filesystem::path GetNodeFullpath(uint32_t nodeIndex) const;
        
        bool IsImageFile(const std::filesystem::path &filepath) const;
        Ref<Texture> GetOrCreateThumbnail(const std::filesystem::path &filepath);
        void ClearThumbnails();

        std::vector<FileTreeNode> m_TreeNodes;
        std::queue<PendingFileLoading> m_PendingAssetLoading;

        int m_ThumbnailSize = 64;
        int m_LastThumbnailSize = 64;

        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;
        std::filesystem::path m_SelectedFileTree;

        std::stack<std::filesystem::path> m_BackwardPathStack;
        std::stack<std::filesystem::path> m_ForwardPathStack;
        std::vector<std::filesystem::path> m_PathEntryList;

        std::unordered_map<std::string, Ref<Texture>> m_Icons;
        std::unordered_map<std::filesystem::path, FileThumbnail> m_Thumbnails;
        
        bool m_NeedsRefresh = false;
    };
}
