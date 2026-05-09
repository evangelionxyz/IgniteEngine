//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once
#ifndef CONTENT_BROWSER_PANEL_HPP
#define CONTENT_BROWSER_PANEL_HPP

#include "ipanel.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/texture.hpp"

// Asset panel
#include "asset_editor_panel.hpp"

#include <filesystem>
#include <stack>
#include <map>
#include <unordered_set>

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
        std::vector<uint32_t> sortedChildren;
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

        static void ReleaseSharedResources();

        virtual void OnGuiRender() override;
        virtual void OnUpdate(float deltaTime) override;

        void LoadProjectFiles(AssetManager *assetManager);
        void RefreshFiles();

    private:
        void RefreshEntryPathList();
        void RefreshAssetTree();
        void LoadAssetTree(const std::filesystem::path &directory);
        void RebuildSortedTreeCache();

        void UIRenderFileTree(FileTreeNode *node);
        void UIRenderFileButton(const std::filesystem::path &item);
        void UIRenderNavigationBar();
        void UIShowAssetAddContext();

        void PruneMissingNodes(uint32_t nodeIndex, const std::filesystem::path &basePath);
        void PruneMissingNodesAlt(uint32_t nodeIndex, const std::filesystem::path &basePath);
        void CollectNodesToDelete(uint32_t nodeIndex, const std::filesystem::path &basePath, std::vector<uint32_t> &nodesToDelete);
        void CollectNodeAndDescendants(uint32_t nodeIndex, std::vector<uint32_t> &nodesToDelete);
        void MarkNodeDeletedRecursive(uint32_t nodeIndex);
        void DeleteSingleNode(uint32_t nodeIndex);
        void UpdateIndicesAfterDeletion(uint32_t deletedIndex);
        void CompactTree();

        void DragDropSource(const std::filesystem::path &filepath);
        bool DuplicateItem(const std::filesystem::path &filepath);
        bool MoveOrCopyPathToDirectory(const std::filesystem::path &sourcePath, const std::filesystem::path &targetDirectory, bool moveItem);
        bool MoveOrCopySelectionToDirectory(const std::filesystem::path &targetDirectory, bool moveItem);
        void UpdateSelection(const std::filesystem::path &filepath);
        std::vector<std::filesystem::path> GetDragSourcePaths(const std::filesystem::path &draggedPath) const;
        void QueueMoveCopyPopup(const std::filesystem::path &draggedPath, const std::filesystem::path &targetDirectory);

        static void OnImportAssetDialog(void *userData, const char * const *fileList, int filter);

        std::filesystem::path GetNodeFullpath(uint32_t nodeIndex) const;
        
        bool IsImageFile(const std::filesystem::path &filepath) const;
        Ref<Texture> GetOrCreateThumbnail(const std::filesystem::path &filepath, bool isDirectory);
        ImVec2 CalculateThumbnailDisplaySize(Ref<Texture> texture, float maxSize) const;
        void StartThumbnailLoad(const std::filesystem::path &filepath);
        void UnloadUnusedThumbnails();
        void ClearThumbnails();

        std::vector<FileTreeNode> m_TreeNodes;
        std::vector<uint32_t> m_SortedRootNodeIndices;
        std::queue<PendingFileLoading> m_PendingAssetLoading;

        AssetEditorPanel *m_AssetEditorPanel;
        AssetManager *m_AssetManager = nullptr;

        int m_ThumbnailSize = 96;
        int m_LastThumbnailSize = 96;

        std::filesystem::path m_BaseDirectory;
        std::filesystem::path m_CurrentDirectory;
        std::filesystem::path m_SelectedFileTree;

        std::stack<std::filesystem::path> m_BackwardPathStack;
        std::stack<std::filesystem::path> m_ForwardPathStack;
        std::vector<std::filesystem::path> m_PathEntryList;

        std::unordered_map<std::string, Ref<Texture>> m_Icons;
        std::unordered_map<std::filesystem::path, FileThumbnail> m_Thumbnails;
        std::queue<std::filesystem::path> m_PendingThumbnailLoads;
        std::unordered_set<std::filesystem::path> m_ThumbnailLoadsInFlight;
        uint64_t m_ThumbnailLoadGeneration = 0;
        
        uint64_t m_CurrentFrame = 0;
        static constexpr uint64_t s_ThumbnailUnloadFrameThreshold = 300; // Unload after 5 seconds at 60fps

        static uint32_t s_InstanceCount;
        static std::unordered_map<std::string, Ref<Texture>> s_SharedIcons;
        static std::unordered_map<std::filesystem::path, FileThumbnail> s_SharedThumbnails;
        static std::queue<std::filesystem::path> s_SharedPendingThumbnailLoads;
        static std::unordered_set<std::filesystem::path> s_SharedThumbnailLoadsInFlight;
        static uint64_t s_SharedThumbnailLoadGeneration;
        static uint64_t s_SharedCurrentFrame;
        
        bool m_NeedsRefresh = false;

        // Modal state for create/rename/delete operations
        bool m_ShowCreateFolderModal = false;
        bool m_ShowCreateScriptModal = false;
        bool m_ShowCreateScriptableObjectModal = false;
        std::string m_PendingScriptableObjectClassName;  // class to instantiate
        std::string m_PendingScriptableObjectFileName;   // suggested file name
        bool m_ShowRenameModal = false;
        bool m_ShowDeleteModal = false;
        bool m_ShowMoveCopyPopup = false;
        std::vector<std::filesystem::path> m_SelectedItems;
        std::vector<std::filesystem::path> m_ActiveDragItems;
        std::vector<std::filesystem::path> m_PendingDragDropSources;
        std::filesystem::path m_PendingDragDropTargetDirectory;
        std::filesystem::path m_PopupTargetPath; // target file/folder for rename/delete
        char m_PopupInputBuffer[1024] = { 0 }; // used for create/rename names
    };
}

#endif