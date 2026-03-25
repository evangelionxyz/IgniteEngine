//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

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
        uint64_t lastFrameUsed = 0;
    };

    class ContentBrowserPanel : public IPanel
    {
    public:
        explicit ContentBrowserPanel(const char *windowTitle, EditorLayer *editor);
        virtual ~ContentBrowserPanel() override;

        virtual void OnGuiRender() override;
        virtual void OnUpdate(float deltaTime) override;

        void LoadProjectFiles();

    private:
        void RenderFileTree(FileTreeNode *node);
        void RefreshEntryPathList();
        void RefreshAssetTree();
        void LoadAssetTree(const std::filesystem::path &directory);

        void UIShowAssetImportContext();

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
        ImVec2 CalculateThumbnailDisplaySize(Ref<Texture> texture, float maxSize) const;
        void StartThumbnailLoad(const std::filesystem::path &filepath);
        void UnloadUnusedThumbnails();
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
        std::queue<std::filesystem::path> m_PendingThumbnailLoads;
        
        uint64_t m_CurrentFrame = 0;
        static constexpr uint64_t s_ThumbnailUnloadFrameThreshold = 300; // Unload after 5 seconds at 60fps
        
        bool m_NeedsRefresh = false;
    };
}
