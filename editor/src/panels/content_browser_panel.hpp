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
        ignite::Path path;
        // path, index
        std::map<ignite::Path, uint32_t> children;
        std::vector<uint32_t> sortedChildren;
        uint32_t parent = static_cast<uint32_t>(-1);
        bool isDeleted = false;

        FileTreeNode(const ignite::Path &path, AssetHandle handle)
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
        void LoadAssetTree(const ignite::Path &directory);
        void RebuildSortedTreeCache();

        void UIRenderFileTree(FileTreeNode *node);
        void UIRenderFileButton(const ignite::Path &item);
        void UIRenderNavigationBar();
        void UIShowAssetAddContext();

        void PruneMissingNodes(uint32_t nodeIndex, const ignite::Path &basePath);
        void PruneMissingNodesAlt(uint32_t nodeIndex, const ignite::Path &basePath);
        void CollectNodesToDelete(uint32_t nodeIndex, const ignite::Path &basePath, std::vector<uint32_t> &nodesToDelete);
        void CollectNodeAndDescendants(uint32_t nodeIndex, std::vector<uint32_t> &nodesToDelete);
        void MarkNodeDeletedRecursive(uint32_t nodeIndex);
        void DeleteSingleNode(uint32_t nodeIndex);
        void UpdateIndicesAfterDeletion(uint32_t deletedIndex);
        void CompactTree();

        void DragDropSource(const ignite::Path &filepath);
        bool DuplicateItem(const ignite::Path &filepath);
        bool MoveOrCopyPathToDirectory(const ignite::Path &sourcePath, const ignite::Path &targetDirectory, bool moveItem);
        bool MoveOrCopySelectionToDirectory(const ignite::Path &targetDirectory, bool moveItem);
        void UpdateSelection(const ignite::Path &filepath);
        std::vector<ignite::Path> GetDragSourcePaths(const ignite::Path &draggedPath) const;
        void QueueMoveCopyPopup(const ignite::Path &draggedPath, const ignite::Path &targetDirectory);

        static void OnImportAssetDialog(void *userData, const char * const *fileList, int filter);

        ignite::Path GetNodeFullpath(uint32_t nodeIndex) const;
        
        bool IsImageFile(const ignite::Path &filepath) const;
        Ref<Texture> GetOrCreateThumbnail(const ignite::Path &filepath, bool isDirectory);
        ImVec2 CalculateThumbnailDisplaySize(Ref<Texture> texture, float maxSize) const;
        void StartThumbnailLoad(const ignite::Path &filepath);
        void UnloadUnusedThumbnails();
        void ClearThumbnails();

        std::vector<FileTreeNode> m_TreeNodes;
        std::vector<uint32_t> m_SortedRootNodeIndices;
        std::queue<PendingFileLoading> m_PendingAssetLoading;

        AssetEditorPanel *m_AssetEditorPanel;
        AssetManager *m_AssetManager = nullptr;

        int m_ThumbnailSize = 96;
        int m_LastThumbnailSize = 96;

        ignite::Path m_BaseDirectory;
        ignite::Path m_CurrentDirectory;
        ignite::Path m_SelectedFileTree;

        std::stack<ignite::Path> m_BackwardPathStack;
        std::stack<ignite::Path> m_ForwardPathStack;
        std::vector<ignite::Path> m_PathEntryList;

        std::unordered_map<std::string, Ref<Texture>> m_Icons;
        std::unordered_map<ignite::Path, FileThumbnail> m_Thumbnails;
        std::queue<ignite::Path> m_PendingThumbnailLoads;
        std::unordered_set<ignite::Path> m_ThumbnailLoadsInFlight;
        uint64_t m_ThumbnailLoadGeneration = 0;
        
        uint64_t m_CurrentFrame = 0;
        static constexpr uint64_t s_ThumbnailUnloadFrameThreshold = 300; // Unload after 5 seconds at 60fps

        static uint32_t s_InstanceCount;
        static std::unordered_map<std::string, Ref<Texture>> s_SharedIcons;
        static std::unordered_map<ignite::Path, FileThumbnail> s_SharedThumbnails;
        static std::queue<ignite::Path> s_SharedPendingThumbnailLoads;
        static std::unordered_set<ignite::Path> s_SharedThumbnailLoadsInFlight;
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
        std::vector<ignite::Path> m_SelectedItems;
        std::vector<ignite::Path> m_ActiveDragItems;
        std::vector<ignite::Path> m_PendingDragDropSources;
        ignite::Path m_PendingDragDropTargetDirectory;
        ignite::Path m_PopupTargetPath; // target file/folder for rename/delete
        char m_PopupInputBuffer[1024] = { 0 }; // used for create/rename names
    };
}

#endif