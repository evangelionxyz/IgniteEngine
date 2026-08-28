
//Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "content_browser_panel.hpp"
#include "ignite/project/project.hpp"
#include "editor_layer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ext/editor_ui.hpp"

#include <format>
#include <algorithm>
#include <ranges>
#include <cstring>
#include <mutex>
#include <SDL3/SDL_dialog.h>

#include <stb_image.h>

namespace ignite
{
    uint32_t ContentBrowserPanel::s_InstanceCount = 0;
    std::unordered_map<std::string, Ref<Texture>> ContentBrowserPanel::s_SharedIcons;
    std::unordered_map<std::filesystem::path, FileThumbnail> ContentBrowserPanel::s_SharedThumbnails;
    std::queue<std::filesystem::path> ContentBrowserPanel::s_SharedPendingThumbnailLoads;
    std::unordered_set<std::filesystem::path> ContentBrowserPanel::s_SharedThumbnailLoadsInFlight;
    uint64_t ContentBrowserPanel::s_SharedThumbnailLoadGeneration = 0;
    uint64_t ContentBrowserPanel::s_SharedCurrentFrame = 0;

    namespace
    {
        static constexpr const char *kContentBrowserPathPayload = "content_browser_path";

        struct ContentBrowserPathPayload
        {
            char path[1024] = {};
        };

        static bool DispatchOpenAssetEditorEvent(AssetHandle handle, const AssetMetaData &metadata)
        {
            if (handle == AssetHandle(0) || metadata.type == AssetType::Invalid)
            {
                return false;
            }

            Application::SubmitToMainThread([handle, metadata]()
            {
                SignalBus::Emit(AssetEditorOpenSignal{ handle, metadata });
            });

            return true;
        }

        static void DispatchCreateAssetEditorEvent(AssetType assetType, const std::filesystem::path &targetDirectory)
        {
            if (assetType == AssetType::Invalid)
            {
                return;
            }

            Application::SubmitToMainThread([assetType, targetDirectory]()
            {
                SignalBus::Emit(AssetEditorCreateSignal{ assetType, targetDirectory });
            });
        }

        static std::filesystem::path BuildUniqueSiblingPath(const std::filesystem::path &sourcePath)
        {
            const std::filesystem::path parentPath = sourcePath.parent_path();
            const bool isDirectory = std::filesystem::is_directory(sourcePath);
            const std::string baseName = isDirectory ? sourcePath.filename().string() : sourcePath.stem().string();
            const std::string extension = isDirectory ? std::string() : sourcePath.extension().string();

            std::filesystem::path candidate = parentPath / (baseName + "_Copy" + extension);
            uint32_t suffix = 1;
            while (std::filesystem::exists(candidate))
            {
                candidate = parentPath / std::format("{}_Copy{}{}", baseName, suffix, extension);
                ++suffix;
            }

            return candidate;
        }

        static bool IsPathWithin(const std::filesystem::path &path, const std::filesystem::path &base)
        {
            auto baseIt = base.begin();
            auto pathIt = path.begin();

            for (; baseIt != base.end(); ++baseIt, ++pathIt)
            {
                if (pathIt == path.end() || *baseIt != *pathIt)
                {
                    return false;
                }
            }

            return true;
        }

        static std::filesystem::path RebasePath(const std::filesystem::path &path, const std::filesystem::path &oldBase, const std::filesystem::path &newBase)
        {
            std::filesystem::path suffix;
            auto oldBaseIt = oldBase.begin();
            auto pathIt = path.begin();

            for (; oldBaseIt != oldBase.end() && pathIt != path.end(); ++oldBaseIt, ++pathIt)
            {
            }

            for (; pathIt != path.end(); ++pathIt)
            {
                suffix /= (*pathIt).string();
            }

            return std::filesystem::path(newBase.string()) / suffix.string();
        }

        static std::filesystem::path BuildUniquePathInDirectory(const std::filesystem::path &sourcePath, const std::filesystem::path &targetDirectory)
        {
            const bool isDirectory = std::filesystem::is_directory(sourcePath);
            const std::string baseName = isDirectory ? sourcePath.filename().string() : sourcePath.stem().string();
            const std::string extension = isDirectory ? std::string() : sourcePath.extension().string();

            std::filesystem::path candidate = targetDirectory / sourcePath.filename();
            uint32_t suffix = 1;
            while (std::filesystem::exists(candidate))
            {
                candidate = targetDirectory / std::format("{}_Copy{}{}", baseName, suffix, extension);
                ++suffix;
            }

            return candidate;
        }

        static const SDL_DialogFileFilter kExtFilters[]
        {
            {"All Supported Assets", "fbx;gltf;glb;png;jpg;jpeg;hdr;wav;mp3;flac;ttf;otf;ixscene;mesh;skmesh;ixmat;ixmat2d;ixsp;ixanim;ixmontage;ixskeleton;ixbs;ixloco;ac;ac2d;anim2d"},
            {"FBX File (.fbx)", "fbx"},
            {"GLTF/GLB File (.gltf;.glb)", "gltf;glb"},

            {"Texture", "png;jpg;jpeg;hdr;exr"},
            {"Audio", "wav;mp3;flac" },
            {"Font", "ttf;otf" },
            {"Ignite Scene (.ixscene)", "ixscene"},
            {"Ignite Static Mesh (.mesh)", "mesh"},
            {"Ignite Skeletal Mesh (.skmesh)", "skmesh"},
            {"Ignite Material (.ixmat)", "ixmat"},
            {"Ignite Material2D (.ixmat2d)", "ixmat2d"},
            {"Ignite Animation (.ixanim)", "ixanim"},
            {"Ignite Animation Montage (.ixmontage)", "ixmontage"},
            {"Ignite Skeleton (.ixskeleton)", "ixskeleton"},
            {"Ignite Blend Space (.ixbs)", "ixbs"},
            {"Ignite Locomotion (.ixloco)", "ixloco"},
            {"Animation Controller (.ac)", "ac"},
            {"Animation Controller 2D (.ac2d)", "ac2d"},
            {"Animation 2D (.anim2d)", "anim2d"}
        };
    }

    ContentBrowserPanel::ContentBrowserPanel(const char *windowTitle, EditorLayer *editor)
        : IPanel(windowTitle, editor), m_AssetManager(nullptr)
    {
        ++s_InstanceCount;

        if (!s_SharedIcons.empty())
        {
            return;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        auto app = Application::GetInstance();

        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        TextureCreateInfo createInfo;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true;
        createInfo.bindless = false;

        s_SharedIcons["scene"] = Texture::Create("resources/ui/editor/ic_editor_scene.png", createInfo, cmd);
        s_SharedIcons["shader"] = Texture::Create("resources/ui/editor/ic_editor_shader.png", createInfo, cmd);
        s_SharedIcons["sprite_sheet"] = Texture::Create("resources/ui/editor/ic_editor_sprite_sheet.png", createInfo, cmd);
        s_SharedIcons["material"] = Texture::Create("resources/ui/editor/ic_editor_material.png", createInfo, cmd);
        s_SharedIcons["material_2d"] = Texture::Create("resources/ui/editor/ic_editor_material_2d.png", createInfo, cmd);
        s_SharedIcons["anim"] = Texture::Create("resources/ui/editor/ic_editor_anim.png", createInfo, cmd);
        s_SharedIcons["font"] = Texture::Create("resources/ui/editor/ic_editor_font.png", createInfo, cmd);
        s_SharedIcons["roll"] = Texture::Create("resources/ui/editor/ic_editor_roll.png", createInfo, cmd);
        s_SharedIcons["arrow"] = Texture::Create("resources/ui/editor/ic_editor_arrow.png", createInfo, cmd);
        s_SharedIcons["add"] = Texture::Create("resources/ui/editor/ic_editor_add.png", createInfo, cmd);
        s_SharedIcons["audio"] = Texture::Create("resources/ui/editor/ic_editor_audio.png", createInfo, cmd);

        s_SharedIcons["skeleton"] = Texture::Create("resources/ui/editor/ic_editor_skeleton.png", createInfo, cmd);
        s_SharedIcons["mesh"] = Texture::Create("resources/ui/editor/ic_editor_sk_mesh.png", createInfo, cmd);
        s_SharedIcons["st_mesh"] = Texture::Create("resources/ui/editor/ic_editor_st_mesh.png", createInfo, cmd);

        s_SharedIcons["anim_ctrl"] = Texture::Create("resources/ui/editor/ic_editor_anim_ctrl.png", createInfo, cmd);
        s_SharedIcons["anim_2d"] = Texture::Create("resources/ui/editor/ic_editor_anim_2d.png", createInfo, cmd);
        s_SharedIcons["anim_ctrl_2d"] = Texture::Create("resources/ui/editor/ic_editor_anim_ctrl_2d.png", createInfo, cmd);

        s_SharedIcons["folder"] = Texture::Create("resources/ui/editor/ic_editor_folder.png", createInfo, cmd);
        s_SharedIcons["unknown"] = Texture::Create("resources/ui/editor/ic_editor_unknown.png", createInfo, cmd);

        cmd->close();
        app->SubmitWorkerCommandList(cmd);
    }

    ContentBrowserPanel::~ContentBrowserPanel()
    {
        if (m_EditorLayer)
        {
            if (m_EditorLayer->m_ContentBrowserPanel == this)
            {
                m_EditorLayer->m_ContentBrowserPanel = nullptr;
            }
            std::erase(m_EditorLayer->m_ContentBrowserPanels, this);
        }

        if (s_InstanceCount > 0)
        {
            --s_InstanceCount;
        }

        m_AssetManager = nullptr;
    }

    void ContentBrowserPanel::ReleaseSharedResources()
    {
        s_SharedThumbnailLoadGeneration++;

        while (!s_SharedPendingThumbnailLoads.empty())
        {
            s_SharedPendingThumbnailLoads.pop();
        }

        s_SharedThumbnailLoadsInFlight.clear();
        s_SharedThumbnails.clear();
        s_SharedIcons.clear();
        s_SharedCurrentFrame = 0;
    }

    void ContentBrowserPanel::LoadProjectFiles()
    {
        m_AssetManager = AssetManager::GetInstance();

        // clear directories
        m_PathEntryList.clear();
        m_TreeNodes.clear();

        m_TreeNodes.emplace_back(".", AssetHandle(0));
        m_SortedRootNodeIndices.clear();

        m_BaseDirectory = m_EditorLayer->GetActiveProject()->GetDirectory();
        m_PathEntryList.push_back(m_BaseDirectory);

        m_CurrentDirectory = m_EditorLayer->GetActiveProject()->GetDirectory();
        RefreshAssetTree();
    }

    void ContentBrowserPanel::RefreshFiles()
    {
        auto project = m_EditorLayer->GetActiveProject();
        project->ValidateAssetRegistry();

        PruneMissingNodes(0, m_EditorLayer->GetActiveProject()->GetDirectory());
        RefreshAssetTree();
        CompactTree();

        const auto &filepath = m_EditorLayer->GetActiveProject()->GetFilepath();
        m_EditorLayer->GetActiveProject()->Serialize(filepath);

        AssetWorker::ReportStatus("Ready");
    }

    void ContentBrowserPanel::UIRenderFileTree(FileTreeNode *node)
    {
        IGN_PROFILE_FUNCTION();

        if (node->path.empty())
            return;

        // Get the node's index in the tree
        const auto nodeIndex = static_cast<uint32_t>(node - m_TreeNodes.data());

        // Build full path using helper function
        const std::filesystem::path assetDir = m_EditorLayer->GetActiveProject()->GetDirectory();
        const std::filesystem::path relativePath = GetNodeFullpath(nodeIndex);
        const std::filesystem::path fullPath = assetDir / relativePath;
        const std::string filename = node->path.filename().string();

        // Check if path exists and is a directory
        bool isDirectory = false;
        if (std::filesystem::exists(fullPath))
        {
            isDirectory = std::filesystem::is_directory(fullPath);
        }

        ImGuiTreeNodeFlags flags = (m_SelectedFileTree == fullPath ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

        if (!isDirectory)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool opened = ImGui::TreeNodeEx(filename.c_str(), flags, "%s", filename.c_str());

        DragDropSource(fullPath);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
        {
            UpdateSelection(fullPath);
            m_SelectedFileTree = fullPath;
        }

        if (isDirectory && ImGui::BeginDragDropTarget())
        {
            bool queuedPopup = false;

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
            {
                if (payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData droppedMetadata = m_AssetManager->GetMetaData(droppedHandle);
                    Project *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
                    if (project && droppedHandle != AssetHandle(0) && droppedMetadata.type != AssetType::Invalid && !droppedMetadata.filepath.empty())
                    {
                        QueueMoveCopyPopup(project->GetProjectFilepath(droppedMetadata.filepath), fullPath);
                        queuedPopup = true;
                    }
                }
            }

            if (!queuedPopup)
            {
                if (const ImGuiPayload *pathPayload = ImGui::AcceptDragDropPayload(kContentBrowserPathPayload))
                {
                    if (pathPayload->DataSize == sizeof(ContentBrowserPathPayload))
                    {
                        const auto *dropData = static_cast<const ContentBrowserPathPayload *>(pathPayload->Data);
                        QueueMoveCopyPopup(std::filesystem::path(dropData->path), fullPath);
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (isDirectory)
            {
                if (fullPath != m_CurrentDirectory)
                {
                    m_BackwardPathStack.push(m_CurrentDirectory);
                    while (!m_ForwardPathStack.empty())
                    {
                        m_ForwardPathStack.pop();
                    }

                    m_SelectedFileTree = m_CurrentDirectory = fullPath;
                }
            }
            else
            {
                auto *project = m_EditorLayer->GetActiveProject().get();
                if (!project)
                    return;

                if (fullPath.extension() == ".cs")
                {
#ifdef PLATFORM_WINDOWS
                    std::string command = std::format("start \"\" \"{}\"", fullPath.generic_string());
                    std::system(command.c_str());
#endif
                }
                else
                {
                    const std::filesystem::path relativeAssetPath = project->GetProjectFilepath(fullPath);
                    AssetHandle handle = m_AssetManager->GetAssetHandle(relativeAssetPath);
                    AssetMetaData metadata = m_AssetManager->GetMetaData(handle);
                    DispatchOpenAssetEditorEvent(handle, metadata);
                }
            }
        }

        if (opened && isDirectory)
        {
            for (const auto &childNodeIndex : node->sortedChildren)
            {
                UIRenderFileTree(&m_TreeNodes[childNodeIndex]);
            }
            ImGui::TreePop();
        }
    }

    void ContentBrowserPanel::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();

        const bool windowOpen = ImGui::Begin(m_WindowTitle.c_str(), &m_IsOpen);
        m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        if (windowOpen)
        {
            if (!m_EditorLayer->GetActiveProject())
            {
                ImGui::End();
                return;
            }

            // Navigation bar
            UIRenderNavigationBar();

            // -------------------------------
            // ---------- FILE TREE ----------
            // -------------------------------
            {
                IGN_PROFILE_SCOPE_COLOR("ContentBrowser::FileTree", 0xCD5C5C);

                // Left side directory tree
                ImGui::BeginChild("left_item_browser", { 300.0f, 0.0f }, ImGuiChildFlags_ResizeX);
                if (!m_TreeNodes.empty())
                {
                    const std::filesystem::path assetDir = m_EditorLayer->GetActiveProject()->GetDirectory();
                    const std::string rootLabel = assetDir.filename().string().empty() ? assetDir.generic_string() : assetDir.filename().string();

                    ImGuiTreeNodeFlags rootFlags = (m_SelectedFileTree == assetDir ? ImGuiTreeNodeFlags_Selected : 0)
                        | ImGuiTreeNodeFlags_OpenOnArrow
                        | ImGuiTreeNodeFlags_SpanAvailWidth
                        | ImGuiTreeNodeFlags_SpanFullWidth
                        | ImGuiTreeNodeFlags_DefaultOpen;

                    const bool rootOpened = ImGui::TreeNodeEx("##content_browser_root", rootFlags, "%s", rootLabel.c_str());

                    DragDropSource(assetDir);

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
                    {
                        UpdateSelection(assetDir);
                        m_SelectedFileTree = assetDir;
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        bool queuedPopup = false;

                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->DataSize == sizeof(AssetHandle))
                            {
                                const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                const AssetMetaData droppedMetadata = m_AssetManager->GetMetaData(droppedHandle);
                                Project *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
                                if (project && droppedHandle != AssetHandle(0) && droppedMetadata.type != AssetType::Invalid && !droppedMetadata.filepath.empty())
                                {
                                    QueueMoveCopyPopup(project->GetProjectFilepath(droppedMetadata.filepath), assetDir);
                                    queuedPopup = true;
                                }
                            }
                        }

                        if (!queuedPopup)
                        {
                            if (const ImGuiPayload *pathPayload = ImGui::AcceptDragDropPayload(kContentBrowserPathPayload))
                            {
                                if (pathPayload->DataSize == sizeof(ContentBrowserPathPayload))
                                {
                                    const auto *dropData = static_cast<const ContentBrowserPathPayload *>(pathPayload->Data);
                                    QueueMoveCopyPopup(std::filesystem::path(dropData->path), assetDir);
                                }
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (assetDir != m_CurrentDirectory)
                        {
                            m_BackwardPathStack.push(m_CurrentDirectory);
                            while (!m_ForwardPathStack.empty())
                            {
                                m_ForwardPathStack.pop();
                            }

                            m_CurrentDirectory = assetDir;
                        }
                    }

                    if (rootOpened)
                    {
                        for (const uint32_t rootNodeIndex : m_SortedRootNodeIndices)
                        {
                            UIRenderFileTree(&m_TreeNodes[rootNodeIndex]);
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::EndChild();
            }

            ImGui::SameLine();


            // -------------------------------
            // ---------- FILE LIST ----------
            // -------------------------------
            {
                IGN_PROFILE_SCOPE_COLOR("ContentBrowser::FileList", 0xCD5C5C);


                if (ImGui::BeginChild("##file_lists", { 0.0f, 0.0f }))
                {
                    // Insert path nodes
                    FileTreeNode *node = m_TreeNodes.data();
                    if (node)
                    {
                        auto &f = m_EditorLayer->GetActiveProject()->GetDirectory();
                        const auto &relativePath = std::filesystem::relative(m_CurrentDirectory.string(), f.string());

                        {
                            IGN_PROFILE_SCOPE_COLOR("ContentBrowser::Submitting paths", 0xCD5C5C);
                            for (const auto &path : relativePath)
                            {
                                if (node->path == relativePath.string())
                                {
                                    break;
                                }

                                if (node->children.contains(path.string()))
                                {
                                    node = &m_TreeNodes[node->children[path.string()]];
                                }
                            }
                        }

                        if (node->children.empty())
                        {
                            UI::DrawCenteredText("Empty folder");
                        }

                        const float spacing = 12.0f;
                        const float contentWidth = ImGui::GetContentRegionAvail().x;
                        const float itemWidth = static_cast<float>(m_ThumbnailSize);

                        const int itemPerRow = std::max(1, static_cast<int>((contentWidth + spacing) / (itemWidth + spacing)));
                        const int itemCount = static_cast<int>(node->sortedChildren.size());
                        const int rowCount = (itemCount + itemPerRow - 1) / itemPerRow;
                        const float rowHeight = static_cast<float>(m_ThumbnailSize) + ImGui::GetTextLineHeightWithSpacing() * 2.0f + spacing;
                        const float cursorStartY = ImGui::GetCursorPosY();

                        ImGuiListClipper clipper;
                        clipper.Begin(rowCount, rowHeight);
                        while (clipper.Step())
                        {
                            ImGui::SetCursorPosY(cursorStartY + static_cast<float>(clipper.DisplayStart) * rowHeight);

                            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                            {
                                const int rowStart = row * itemPerRow;
                                const int rowEnd = std::min(rowStart + itemPerRow, itemCount);

                                for (int i = rowStart; i < rowEnd; ++i)
                                {
                                    const auto childNodeIndex = node->sortedChildren[static_cast<size_t>(i)];
                                    const auto &item = m_TreeNodes[childNodeIndex].path;

                                    if (i > rowStart)
                                    {
                                        ImGui::SameLine(0.0f, spacing);
                                    }

                                    ImGui::PushID(item.generic_string().c_str());
                                    UIRenderFileButton(item);
                                    ImGui::PopID();
                                }
                            }
                        }

                        ImGui::SetCursorPosY(cursorStartY);
                        const ImVec2 remainingSize = ImGui::GetContentRegionMax();
                        ImGui::Dummy(ImVec2(remainingSize.x, std::max(remainingSize.y, 50.0f)));

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                            {
                                const size_t count = payload->DataSize / sizeof(UUID);
                                const auto droppedUUIDs = static_cast<const UUID *>(payload->Data);
                                Scene *activeScene = m_EditorLayer ? m_EditorLayer->GetActiveScene().get() : nullptr;
                                Project *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;

                                if (activeScene && project)
                                {
                                    for (size_t i = 0; i < count; ++i)
                                    {
                                        Entity entity = SceneManager::GetEntity(activeScene, droppedUUIDs[i]);
                                        if (entity.IsValid())
                                        {
                                            std::string entityName = entity.GetName();
                                            if (entityName.empty()) entityName = "Prefab";
                                            std::filesystem::path targetPath = BuildUniqueSiblingPath(m_CurrentDirectory / (entityName + ".ixprefab"));

                                            Ref<Prefab> prefab = Prefab::CreateFromEntity(entity, activeScene, project);
                                            if (prefab && prefab->Serialize(targetPath))
                                            {
                                                AssetHandle handle = m_AssetManager->ImportAsset(targetPath);
                                                if (handle != AssetHandle(0) && !entity.HasComponent<PrefabComponent>())
                                                {
                                                    entity.AddComponent<PrefabComponent>(handle, prefab->GetRootEntityUUID());
                                                }
                                                RefreshFiles();
                                            }
                                        }
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }

                    // ==== Context menu ====
                    UIRenderContextMenu();
                }

                ImGui::EndChild();
            }

            // ==== Popup modals ====
            UIRenderPopupModals();
        }

        ImGui::End();
    }

    void ContentBrowserPanel::OnUpdate(float deltaTime)
    {
        // Increment frame counter
        s_SharedCurrentFrame++;

        // Check if thumbnail size changed and clear thumbnails if needed
        if (m_ThumbnailSize != m_LastThumbnailSize)
        {
            ClearThumbnails();
            m_LastThumbnailSize = m_ThumbnailSize;
        }

        // Load only 1 thumbnail per frame
        if (!s_SharedPendingThumbnailLoads.empty() && s_SharedCurrentFrame % 3 == 0)
        {
            std::filesystem::path filepath = s_SharedPendingThumbnailLoads.front();
            s_SharedPendingThumbnailLoads.pop();

            // Check if still needs loading (not already loaded)
            auto it = s_SharedThumbnails.find(filepath);
            if (it != s_SharedThumbnails.end() && it->second.thumbnail == nullptr)
            {
                StartThumbnailLoad(filepath);
            }
            else
            {
                s_SharedThumbnailLoadsInFlight.erase(filepath);
            }
        }

        // Unload unused thumbnails every 60 frames (once per second at 60fps)
        if (s_SharedCurrentFrame % 60 == 0)
        {
            UnloadUnusedThumbnails();
        }
    }

    void ContentBrowserPanel::UIRenderContextMenu()
    {
        if (ImGui::BeginPopupContextWindow("##content_browser_context_menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoReopen | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::BeginMenu("Create"))
            {
                if (ImGui::MenuItem("New Folder"))
                {
                    // show create folder modal
                    m_ShowCreateFolderModal = true;
                }

                if (ImGui::MenuItem("C# Script"))
                {
                    m_ShowCreateScriptModal = true;
                }

                if (ImGui::MenuItem("Prefab"))
                {
                    CreateNewPrefab();
                }

                if (ImGui::MenuItem("Sprite Sheet"))
                {
                    DispatchCreateAssetEditorEvent(AssetType::SpriteSheet, m_CurrentDirectory);
                }

                if (ImGui::MenuItem("Widget"))
                {
                    DispatchCreateAssetEditorEvent(AssetType::Widget, m_CurrentDirectory);
                }

                if (ImGui::BeginMenu("Animation"))
                {
                    if (ImGui::MenuItem("Blend Space 2D"))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::BlendSpace, m_CurrentDirectory);
                    }

                    if (ImGui::MenuItem("Animator Controller"))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::AnimatorController, m_CurrentDirectory);
                    }

                    if (ImGui::MenuItem("Animation 2D"))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::Animation2D, m_CurrentDirectory);
                    }

                    if (ImGui::MenuItem("Animator Controller 2D"))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::AnimatorController2D, m_CurrentDirectory);
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Material"))
                {
                    if (ImGui::MenuItem("Material2D"))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::Material2D, m_CurrentDirectory);
                    }

                    if (ImGui::MenuItem("Material"))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::Material, m_CurrentDirectory);
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Terrain"))
                {
                    DispatchCreateAssetEditorEvent(AssetType::Terrain, m_CurrentDirectory);
                }

                // ----- Scriptable Object submenu (dynamic, from [CreateAssetMenu]) -----
                {
                    ScriptEngine *scriptEngine = ScriptEngine::GetInstance();
                    const auto *menuEntries = scriptEngine
                        ? &scriptEngine->GetScriptableObjectMenuEntries()
                        : nullptr;

                    if (menuEntries && !menuEntries->empty())
                    {
                        ImGui::Separator();
                        if (ImGui::BeginMenu("Scriptable Object"))
                        {
                            for (const auto &entry : *menuEntries)
                            {
                                const std::string &label = entry.menuName.empty() ? entry.className : entry.menuName;
                                if (ImGui::MenuItem(label.c_str()))
                                {
                                    m_PendingScriptableObjectClassName = entry.className;
                                    m_PendingScriptableObjectFileName = entry.fileName;
                                    strncpy(m_PopupInputBuffer, entry.fileName.c_str(), sizeof(m_PopupInputBuffer) - 1);
                                    // strncpy_s(m_PopupInputBuffer, sizeof(m_PopupInputBuffer), entry.fileName.c_str(), sizeof(m_PopupInputBuffer) - 1);
                                    m_ShowCreateScriptableObjectModal = true;
                                }
                            }
                            ImGui::EndMenu();
                        }
                    }
                }

                ImGui::EndMenu();
            }

            // Open file dialog to import assets into the current directory
            if (ImGui::BeginMenu("Import"))
            {
                if (ImGui::MenuItem("Import to this current directory"))
                {
                    SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                        Application::GetInstance()->GetWindow()->GetWindowHandle(),
                        kExtFilters, IM_ARRAYSIZE(kExtFilters),
                        nullptr, true
                    );
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Thumbnail Size"))
            {
                if (ImGui::MenuItem("Small")) m_ThumbnailSize = 64;
                if (ImGui::MenuItem("Medium")) m_ThumbnailSize = 72;
                if (ImGui::MenuItem("Large")) m_ThumbnailSize = 96;
                ImGui::EndMenu();
            }

#ifdef PLATFORM_WINDOWS
            if (ImGui::MenuItem("Open in File Explorer"))
            {
                std::string command = std::format("explorer {}", m_CurrentDirectory.string());
                std::system(command.c_str());
            }
#endif
            ImGui::EndPopup();
        }
    }

    void ContentBrowserPanel::UIRenderPopupModals()
    {
        // Create Folder Modal
        if (m_ShowCreateFolderModal)
        {
            ImGui::OpenPopup("Create Folder");
            m_ShowCreateFolderModal = false;
        }

        static bool focusCreateFolder = true;
        if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create folder in: %s", m_CurrentDirectory.generic_string().c_str());
            ImGui::Spacing();
            if (focusCreateFolder)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateFolder = false;
            }
            const bool submitByEnter = ImGui::InputText("Folder Name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Separator();
            if (submitByEnter || ImGui::Button("Create"))
            {
                std::string name = m_PopupInputBuffer;
                if (!name.empty())
                {
                    std::filesystem::path newPath = m_CurrentDirectory / name;
                    if (!std::filesystem::exists(newPath))
                    {
                        std::error_code ec;
                        std::filesystem::create_directory(newPath.string(), ec);
                        if (!ec)
                        {
                            Application::SubmitToMainThread([this]() { RefreshFiles(); });
                        }
                    }
                }
                // clear
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            focusCreateFolder = true;
        }

        // Create C# Script Modal
        if (m_ShowCreateScriptModal)
        {
            ImGui::OpenPopup("Create C# Script");
            m_ShowCreateScriptModal = false;
        }

        static bool focusCreateScript = true;
        if (ImGui::BeginPopupModal("Create C# Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create script in: %s", m_CurrentDirectory.generic_string().c_str());
            ImGui::Spacing();
            if (focusCreateScript)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateScript = false;
            }
            const bool submitByEnter = ImGui::InputText("Script Name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Separator();
            if (submitByEnter || ImGui::Button("Create"))
            {
                std::string name = m_PopupInputBuffer;
                if (!name.empty())
                {
                    if (!name.ends_with(".cs")) name += ".cs";
                    std::filesystem::path newPath = m_CurrentDirectory / name;
                    if (!std::filesystem::exists(newPath))
                    {
                        m_EditorLayer->GetActiveProject()->CreateCSharpScript(newPath);
                        Application::SubmitToMainThread([this]() { RefreshFiles(); });
                    }
                }
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            focusCreateScript = true;
        }

        // Create Scriptable Object Modal
        if (m_ShowCreateScriptableObjectModal)
        {
            ImGui::OpenPopup("Create Scriptable Object");
            m_ShowCreateScriptableObjectModal = false;
        }

        static bool focusCreateSO = true;
        if (ImGui::BeginPopupModal("Create Scriptable Object", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Class: %s", m_PendingScriptableObjectClassName.c_str());
            ImGui::Text("Location: %s", m_CurrentDirectory.generic_string().c_str());
            ImGui::Spacing();
            if (focusCreateSO)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateSO = false;
            }
            const bool submitByEnter = ImGui::InputText("Asset Name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Separator();
            if (submitByEnter || ImGui::Button("Create"))
            {
                const std::string assetName = m_PopupInputBuffer;
                if (!assetName.empty() && !m_PendingScriptableObjectClassName.empty())
                {
                    m_EditorLayer->GetActiveProject()->CreateScriptableObject(
                        m_PendingScriptableObjectClassName,
                        assetName,
                        m_CurrentDirectory);

                    Application::SubmitToMainThread([this]() { RefreshFiles(); });
                }
                m_PopupInputBuffer[0] = '\0';
                m_PendingScriptableObjectClassName.clear();
                m_PendingScriptableObjectFileName.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_PopupInputBuffer[0] = '\0';
                m_PendingScriptableObjectClassName.clear();
                m_PendingScriptableObjectFileName.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            focusCreateSO = true;
        }

        // Rename Modal
        if (m_ShowRenameModal)
        {
            ImGui::OpenPopup("Rename Item");
            m_ShowRenameModal = false;
        }

        static bool focusRename = true;
        if (ImGui::BeginPopupModal("Rename Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (!m_SelectedItems.empty())
            {
                // ONLY Support renaming a single item for now
                // TODO: Support renaming multiple items at once (e.g., "NewName (1)", "NewName (2)", etc.)
                auto &itemPath = m_SelectedItems.front();

                ImGui::Text("Rename: %s", itemPath.filename().generic_string().c_str());
                ImGui::Spacing();
                if (focusRename)
                {
                    ImGui::SetKeyboardFocusHere();
                    focusRename = false;
                }
                const bool submitByEnter = ImGui::InputText("New Name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::Separator();
                const bool submitByButton = ImGui::Button("Rename");
                if (submitByEnter || submitByButton)
                {
                    std::string newName = m_PopupInputBuffer;
                    if (!newName.empty())
                    {
                        std::filesystem::path target = itemPath;
                        const bool isDirectory = std::filesystem::is_directory(target);
                        if (!isDirectory && !target.extension().empty())
                        {
                            std::filesystem::path proposed(newName);
                            newName = proposed.stem().string();
                            if (!newName.empty())
                            {
                                newName += target.extension().string();
                            }
                        }

                        std::filesystem::path dest = target.parent_path() / newName;

                        std::error_code ec;
                        std::filesystem::rename(target.string(), dest.string(), ec);
                        if (!ec)
                        {
                            // Keep metadata/settings pair in sync.
                            const std::filesystem::path targetMeta = target.string() + ".meta";
                            const std::filesystem::path destMeta = dest.string() + ".meta";
                            if (std::filesystem::exists(targetMeta))
                            {
                                std::error_code ecMetaRename;
                                std::filesystem::rename(targetMeta.string(), destMeta.string(), ecMetaRename);
                            }

                            Project *project = m_EditorLayer->GetActiveProject().get();
                            if (project && m_AssetManager)
                            {
                                const std::filesystem::path oldRelativePath = project->GetProjectFilepath(target);
                                const std::filesystem::path newRelativePath = project->GetProjectFilepath(dest);

                                if (isDirectory)
                                {
                                    auto &registry = m_AssetManager->GetAssetAssetRegistry();
                                    for (const auto &[handle, metadata] : registry)
                                    {
                                        if (handle == AssetHandle(0) || metadata.filepath.empty() || !IsPathWithin(metadata.filepath.string(), oldRelativePath.string()))
                                        {
                                            continue;
                                        }

                                        AssetMetaData updatedMetadata = metadata;
                                        updatedMetadata.filepath = RebasePath(metadata.filepath.string(), oldRelativePath.string(), newRelativePath.string());
                                        m_AssetManager->AssignMetaData(handle, updatedMetadata);
                                    }
                                }
                                else
                                {
                                    const AssetHandle existingHandle = m_AssetManager->GetAssetHandle(oldRelativePath);
                                    if (existingHandle != AssetHandle(0))
                                    {
                                        AssetMetaData metadata = m_AssetManager->GetMetaData(existingHandle);
                                        metadata.filepath = newRelativePath;
                                        m_AssetManager->AssignMetaData(existingHandle, metadata);
                                    }
                                }
                            }

                            Application::SubmitToMainThread([this]() { RefreshFiles(); });
                        }
                    }
                    m_PopupInputBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_PopupInputBuffer[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
        else
        {
            focusRename = true;
        }

        // Delete Confirmation Modal
        if (m_ShowDeleteModal)
        {
            ImGui::OpenPopup("Delete Item");
            m_ShowDeleteModal = false;
        }

        if (ImGui::BeginPopupModal("Delete Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const size_t itemCount = m_SelectedItems.size();

            ImGui::Text("Are you sure you want to delete: %zu item(s) ?", itemCount);
            ImGui::Spacing();
            ImGui::Separator();

            for (const auto &item : m_SelectedItems)
            {
                const auto filename = item.filename().generic_string();
                ImGui::Text("%s", filename.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();

            if (ImGui::Button("Delete"))
            {

                AssetWorker::SubmitJob([this]()
                {
                    bool genCSProjectRequested = false;

                    struct FileToRemove
                    {
                        AssetHandle handle = AssetHandle(0);
                        std::filesystem::path path;
                    };

                    std::vector<FileToRemove> filesToRemove;
                    filesToRemove.reserve(256);

                    for (const auto &itemPath : m_SelectedItems)
                    {
                        std::error_code ec;
                        if (std::filesystem::is_directory(itemPath))
                        {
                            auto project = m_EditorLayer->GetActiveProject();

                            if (project && m_AssetManager)
                            {
                                for (const auto &entry : std::filesystem::recursive_directory_iterator(itemPath.string()))
                                {
                                    if (!entry.is_regular_file())
                                        continue;

                                    const std::filesystem::path &filepath = entry.path().string();
                                    const std::filesystem::path relativePath = project->GetProjectFilepath(filepath);
                                    const AssetHandle handle = m_AssetManager->GetAssetHandle(relativePath);
                                    if (handle != AssetHandle(0))
                                    {
                                        filesToRemove.emplace_back(FileToRemove{ handle, filepath });
                                    }

                                    // Also remove .meta from asset system if needed
                                    if (filepath.extension() != ".meta")
                                    {
                                        std::filesystem::path metaPath = filepath.string() + ".meta";
                                        if (std::filesystem::exists(metaPath))
                                        {
                                            const std::filesystem::path metaRelative = project->GetProjectFilepath(metaPath);
                                            const AssetHandle metaHandle = m_AssetManager->GetAssetHandle(metaRelative);
                                            if (metaHandle != AssetHandle(0))
                                            {
                                                filesToRemove.emplace_back(FileToRemove{ metaHandle, metaPath });
                                            }
                                        }
                                    }
                                }
                            }

                            std::filesystem::remove_all(itemPath.string(), ec);
                        }
                        else
                        {
                            Project *project = m_EditorLayer->GetActiveProject().get();

                            // Remove the item from the OS File System
                            std::filesystem::remove(itemPath.string(), ec);


                            // Remove the side-car file (.meta file)
                            const std::filesystem::path metaPath = itemPath.string() + ".meta";
                            if (std::filesystem::exists(metaPath))
                            {
                                std::error_code metaEc;
                                std::filesystem::remove(metaPath.string(), metaEc);

                                // Push to the queue
                                const std::filesystem::path metaRelative = project->GetProjectFilepath(metaPath);
                                const AssetHandle metaHandle = m_AssetManager->GetAssetHandle(metaRelative);
                                if (metaHandle != AssetHandle(0))
                                {
                                    filesToRemove.emplace_back(FileToRemove{ metaHandle, metaPath });
                                }
                            }


                            // Push to the queue
                            const std::filesystem::path relativePath = project->GetProjectFilepath(itemPath);
                            const AssetHandle handle = m_AssetManager->GetAssetHandle(relativePath);
                            if (handle != AssetHandle(0))
                            {
                                filesToRemove.emplace_back(FileToRemove{ handle, itemPath });
                            }
                        }

                        if (!ec)
                        {
                            if (itemPath.extension().string() == ".cs")
                            {
                                genCSProjectRequested = true;
                            }
                        }
                    }

                    int counter = 0;
                    for (auto &[handle, path] : filesToRemove)
                    {
                        if (handle != AssetHandle(0))
                        {
                            m_AssetManager->UnloadAsset(handle);
                            m_AssetManager->RemoveAsset(handle);
                        }

                        AssetWorker::ReportStatus(std::format("Unload asset: {}", path.generic_string()),
                            counter / (float)filesToRemove.size());

                        counter++;
                    }

                    Application::SubmitToMainThread([this, &genCSProjectRequested]()
                    {
                        if (genCSProjectRequested)
                        {
                            m_EditorLayer->GetActiveProject()->RegenerateCSharpProject();
                        }

                        RefreshFiles();
                    });
                });
                

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (m_ShowMoveCopyPopup)
        {
            ImGui::OpenPopup("Move/Copy Item");
            m_ShowMoveCopyPopup = false;
        }

        if (ImGui::BeginPopup("Move/Copy Item"))
        {
            const size_t sourceCount = m_PendingDragDropSources.size();
            if (sourceCount == 1)
            {
                ImGui::Text("%s", m_PendingDragDropSources.front().filename().generic_string().c_str());
            }
            else
            {
                ImGui::Text("%zu items", sourceCount);
            }

            ImGui::TextDisabled("to %s", m_PendingDragDropTargetDirectory.filename().generic_string().c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Move"))
            {
                MoveOrCopySelectionToDirectory(m_PendingDragDropTargetDirectory, true);
                m_PendingDragDropSources.clear();
                m_PendingDragDropTargetDirectory.clear();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Copy"))
            {
                MoveOrCopySelectionToDirectory(m_PendingDragDropTargetDirectory, false);
                m_PendingDragDropSources.clear();
                m_PendingDragDropTargetDirectory.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
        else if (!m_PendingDragDropSources.empty())
        {
            m_PendingDragDropSources.clear();
            m_PendingDragDropTargetDirectory.clear();
        }
    }

    void ContentBrowserPanel::RefreshEntryPathList()
    {
        if (!m_PathEntryList.empty())
        {
            m_PathEntryList.erase(m_PathEntryList.begin() + 1, m_PathEntryList.end());
        }

        const auto &relativePath = std::filesystem::relative(m_CurrentDirectory.string(), m_EditorLayer->GetActiveProject()->GetAssetDirectory().string());
        auto currentDir = m_EditorLayer->GetActiveProject()->GetAssetDirectory();

        for (const auto &p : relativePath)
        {
            const std::string &pString = p.string();
            if (pString != ".")
            {
                currentDir /= p.string();
                m_PathEntryList.push_back(currentDir);
            }
        }
    }

    void ContentBrowserPanel::RefreshAssetTree()
    {
        const std::filesystem::path &assetPath = m_EditorLayer->GetActiveProject()->GetDirectory();
        LoadAssetTree(assetPath);
        RebuildSortedTreeCache();
    }

    void ContentBrowserPanel::RebuildSortedTreeCache()
    {
        m_SortedRootNodeIndices.clear();

        if (m_TreeNodes.empty())
        {
            return;
        }

        const std::filesystem::path assetDir = m_EditorLayer->GetActiveProject()->GetDirectory();

        for (uint32_t i = 0; i < m_TreeNodes.size(); ++i)
        {
            m_TreeNodes[i].sortedChildren.clear();
            m_TreeNodes[i].sortedChildren.reserve(m_TreeNodes[i].children.size());

            for (const auto &childNodeIndex : m_TreeNodes[i].children | std::views::values)
            {
                if (childNodeIndex < m_TreeNodes.size() && !m_TreeNodes[childNodeIndex].isDeleted)
                {
                    m_TreeNodes[i].sortedChildren.push_back(childNodeIndex);
                }
            }

            std::ranges::sort(m_TreeNodes[i].sortedChildren, [this, &assetDir](uint32_t leftIndex, uint32_t rightIndex)
            {
                const std::filesystem::path leftPath = assetDir / GetNodeFullpath(leftIndex);
                const std::filesystem::path rightPath = assetDir / GetNodeFullpath(rightIndex);

                const bool leftIsDirectory = std::filesystem::exists(leftPath) && std::filesystem::is_directory(leftPath);
                const bool rightIsDirectory = std::filesystem::exists(rightPath) && std::filesystem::is_directory(rightPath);

                if (leftIsDirectory != rightIsDirectory)
                {
                    return leftIsDirectory;
                }

                return m_TreeNodes[leftIndex].path.generic_string() < m_TreeNodes[rightIndex].path.generic_string();
            });
        }

        for (uint32_t i = 1; i < m_TreeNodes.size(); ++i)
        {
            if (m_TreeNodes[i].parent == 0 && !m_TreeNodes[i].isDeleted)
            {
                m_SortedRootNodeIndices.push_back(i);
            }
        }

        std::ranges::sort(m_SortedRootNodeIndices, [this, &assetDir](uint32_t leftIndex, uint32_t rightIndex)
        {
            const std::filesystem::path leftPath = assetDir / GetNodeFullpath(leftIndex);
            const std::filesystem::path rightPath = assetDir / GetNodeFullpath(rightIndex);

            const bool leftIsDirectory = std::filesystem::exists(leftPath) && std::filesystem::is_directory(leftPath);
            const bool rightIsDirectory = std::filesystem::exists(rightPath) && std::filesystem::is_directory(rightPath);

            if (leftIsDirectory != rightIsDirectory)
            {
                return leftIsDirectory;
            }

            return m_TreeNodes[leftIndex].path.generic_string() < m_TreeNodes[rightIndex].path.generic_string();
        });
    }

    void ContentBrowserPanel::LoadAssetTree(const std::filesystem::path &directory)
    {
        const std::filesystem::path assetPath = m_EditorLayer->GetActiveProject()->GetDirectory();

        for (const auto &entry : std::filesystem::directory_iterator(directory.string()))
        {
            if (entry.path().filename() == "Bin" || entry.path().filename() == "obj" || entry.path().filename() == ".vs" ||
                entry.path().filename() == "AssetRegistry.ixreg" || entry.path().extension() == ".slnx" ||
                entry.path().extension() == ".ixproj" || entry.path().filename() == "premake5.lua")
            {
                continue;
            }

            if (!entry.is_directory() && entry.path().extension() == ".meta")
            {
                continue;
            }

            const auto &relativePath = std::filesystem::relative(entry.path().string(), assetPath.string());
            uint32_t currentNodeIndex = 0;

            for (const auto &path : relativePath)
            {
                // Skip dot file/directory
                if (relativePath.string()[0] == '.')
                    continue;

                if (m_TreeNodes.empty())
                    continue;

                const auto it = m_TreeNodes[currentNodeIndex].children.find(path.generic_string());
                if (it != m_TreeNodes[currentNodeIndex].children.end())
                {
                    currentNodeIndex = it->second;
                }
                else
                {
                    AssetHandle assetHandle = AssetHandle(0);
                    if (!std::filesystem::is_directory(path.string()) && path.has_extension())
                    {
                        std::string relPath = relativePath.generic_string();
                        assetHandle = m_AssetManager->GetAssetHandle(relPath);
                        AssetType assetType = GetAssetTypeFromExtension(relativePath.extension().generic_string());

                        // not registered yet
                        // (insert the metadata and generate the asset handle)
                        if (assetHandle == AssetHandle(0))
                        {
                            assetHandle = AssetHandle();
                            AssetMetaData metadata;
                            metadata.type = assetType;
                            metadata.filepath = relPath;
                            m_AssetManager->AssignMetaData(assetHandle, metadata);
                        }
                    }

                    FileTreeNode newNode(path.string(), assetHandle);
                    newNode.parent = currentNodeIndex;

                    m_TreeNodes.push_back(newNode);
                    m_TreeNodes[currentNodeIndex].children[path.string()] = static_cast<int>(m_TreeNodes.size()) - 1;
                    currentNodeIndex = static_cast<int>(m_TreeNodes.size()) - 1;
                }
            }

            if (entry.is_directory())
            {
                LoadAssetTree(entry.path().string());
            }
        }
    }

    void ContentBrowserPanel::UIRenderFileButton(const std::filesystem::path &item)
    {
        IGN_PROFILE_SCOPE_COLOR("ContentBrowser::UIRenderFileButton", 0xCD5C5C);

        std::filesystem::path path = m_CurrentDirectory / item;
        std::error_code statusError;
        const std::filesystem::file_status fileStatus = std::filesystem::status(path.string(), statusError);
        if (statusError || !std::filesystem::exists(fileStatus))
            return;

        const bool isDirectory = std::filesystem::is_directory(fileStatus);
        const AssetType itemAssetType = GetAssetTypeFromExtension(item.extension().string());

        Ref<Texture> icon = GetOrCreateThumbnail(path, isDirectory);
        if (!icon)
        {
            // fallback, because it is generated asynchronously
            icon = s_SharedIcons["unknown"];
        }

        ImGui::BeginGroup();

        // Keep a fixed clickable area and draw the image separately with preserved aspect ratio
        const auto maxSize = static_cast<float>(m_ThumbnailSize);
        const ImVec2 buttonSize(maxSize, maxSize);
        const ImVec2 buttonMin = ImGui::GetCursorScreenPos();
        const ImVec2 buttonMax(buttonMin.x + buttonSize.x, buttonMin.y + buttonSize.y);

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton(item.string().c_str(), buttonSize);

        const bool isActive = ImGui::IsItemActive();
        const bool isHovered = ImGui::IsItemHovered();
        const bool isSelected = std::ranges::find(m_SelectedItems, path) != m_SelectedItems.end();

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
        {
            UpdateSelection(path);
            m_SelectedFileTree = path;
        }

        const ImU32 buttonColor = (isActive
            ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
            : (isHovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : 0x0));

        drawList->AddRectFilled(buttonMin, buttonMax, buttonColor, ImGui::GetStyle().FrameRounding);

        const ImVec2 displaySize = CalculateThumbnailDisplaySize(icon, maxSize);
        const ImVec2 imageMin =
        {
            buttonMin.x + (maxSize - displaySize.x) * 0.5f,
            buttonMin.y + (maxSize - displaySize.y) * 0.5f
        };

        const ImVec2 imageMax(imageMin.x + displaySize.x, imageMin.y + displaySize.y);

        ImTextureID iconId = reinterpret_cast<ImTextureID>(icon->GetHandle().Get());
        drawList->AddImage(iconId, imageMin, imageMax);

        if (isSelected)
        {
            const ImU32 overlayFill = ImGui::GetColorU32(ImVec4(0.20f, 0.45f, 0.85f, 0.22f));
            const ImU32 overlayBorder = ImGui::GetColorU32(ImGuiCol_HeaderActive);
            drawList->AddRectFilled(buttonMin, buttonMax, overlayFill, ImGui::GetStyle().FrameRounding);
            drawList->AddRect(buttonMin, buttonMax, overlayBorder, ImGui::GetStyle().FrameRounding, 0, 2.0f);
        }

        if (ImGui::IsItemHovered())
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (isDirectory)
                {
                    if (path != m_CurrentDirectory)
                    {
                        m_BackwardPathStack.push(m_CurrentDirectory);
                        while (!m_ForwardPathStack.empty())
                        {
                            m_ForwardPathStack.pop();
                        }

                        m_CurrentDirectory = path;
                    }
                }
                else if (m_EditorLayer && m_EditorLayer->GetActiveProject())
                {
                    auto *project = m_EditorLayer->GetActiveProject().get();
                    if (path.extension() == ".cs")
                    {
#ifdef PLATFORM_WINDOWS
                        std::string command = std::format("start \"\" \"{}\"", path.generic_string());
                        std::system(command.c_str());
#endif
                    }
                    else
                    {
                        const std::filesystem::path relativeAssetPath = project->GetProjectFilepath(path);

                        AssetHandle handle = m_AssetManager->GetAssetHandle(relativeAssetPath);
                        AssetMetaData metadata = m_AssetManager->GetMetaData(handle);
                        DispatchOpenAssetEditorEvent(handle, metadata);
                    }
                }
            }
        }

        // Item Popup context
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Open"))
            {
                if (isDirectory)
                {
                    if (path != m_CurrentDirectory)
                    {
                        m_BackwardPathStack.push(m_CurrentDirectory);
                        while (!m_ForwardPathStack.empty())
                        {
                            m_ForwardPathStack.pop();
                        }

                        m_CurrentDirectory = path;
                    }
                }
                else
                {
                    auto *project = m_EditorLayer->GetActiveProject().get();
                    const std::filesystem::path relativeAssetPath = project->GetProjectFilepath(path);

                    AssetHandle handle = m_AssetManager->GetAssetHandle(relativeAssetPath);
                    AssetMetaData metadata = m_AssetManager->GetMetaData(handle);
                    if (!DispatchOpenAssetEditorEvent(handle, metadata))
                    {
                        // If not handled
                        // Windows
                        std::string command = std::format("\"{}\"", path.generic_string());
                        std::system(command.c_str());
                    }
                }
            }

            if (!isDirectory)
            {
                if (ImGui::MenuItem("Import"))
                {
                    m_AssetManager->ImportAsset(path);
                }
            }

            // Actions
            {
                // Rename
                if (ImGui::MenuItem("Rename"))
                {
                    // prefill buffer with filename
                    std::string fname = std::filesystem::is_directory(path) ? path.filename().generic_string() : path.stem().generic_string();
                    std::strncpy(m_PopupInputBuffer, fname.c_str(), sizeof(m_PopupInputBuffer) - 1);
                    m_ShowRenameModal = true;
                }

                if (!isDirectory && ImGui::MenuItem("Duplicate"))
                {
                    // TODO: Implement duplicate for multiple items
                    DuplicateItem(path);
                }

                // Delete
                if (ImGui::MenuItem("Delete"))
                {
                    m_ShowDeleteModal = true;
                }
            }


            // Special actions for certain asset types
            if (!isDirectory)
            {
                switch (itemAssetType)
                {
                    case AssetType::Scene:
                    {
                        if (ImGui::MenuItem("Set As Default Scene"))
                        {
                            auto project = m_EditorLayer->GetActiveProject();
                            if (project)
                            {
                                m_AssetManager->ImportAsset(path);
                                AssetHandle handle = m_AssetManager->GetAssetHandle(path);
                                project->SetDefaultScene(handle);

                                project->Serialize(project->GetFilepath());
                            }
                        }
                        break; // !Scene
                    }
                    case AssetType::SkeletalAnimation:
                    {
                        if (ImGui::MenuItem("Create Animation Montage"))
                        {
                            Project *project = m_EditorLayer->GetActiveProject().get();
                            if (project && m_AssetManager)
                            {
                                const std::filesystem::path relativeAnimPath = project->GetProjectFilepath(path);
                                const AssetHandle animHandle = m_AssetManager->GetAssetHandle(relativeAnimPath);
                                const AssetMetaData animationMetadata = m_AssetManager->GetMetaData(animHandle);

                                if (animHandle != AssetHandle(0) && animationMetadata.type == AssetType::SkeletalAnimation)
                                {
                                    Ref<AnimationMontage> montage = CreateRef<AnimationMontage>();
                                    montage->name = std::format("{}_Montage", path.stem().string());
                                    montage->SetAnimationHandle(animHandle);

                                    Ref<SkeletalAnimation> animation = m_AssetManager->GetAsset<SkeletalAnimation>(animHandle);
                                    if (animation)
                                    {
                                        montage->SetSkeletonHandle(animation->GetSkeletonHandle());
                                    }

                                    std::filesystem::path montagePath = path.parent_path() / (path.stem().string() + GetAssetExtensionFromType(AssetType::AnimationMontage));
                                    uint32_t suffix = 1;
                                    while (std::filesystem::exists(montagePath))
                                    {
                                        montagePath = path.parent_path() / std::format("{}_{}{}", path.stem().string(), suffix, GetAssetExtensionFromType(AssetType::AnimationMontage));
                                        ++suffix;
                                    }

                                    if (montage->Serialize(montagePath))
                                    {
                                        const AssetHandle montageHandle = AssetHandle();
                                        montage->handle = montageHandle;
                                        montage->SetDirtyFlag(false);
                                        montage->SetReadyFlag(true);

                                        AssetMetaData montageMetaData;
                                        montageMetaData.type = AssetType::AnimationMontage;
                                        montageMetaData.filepath = project->GetProjectFilepath(montagePath);

                                        m_AssetManager->AssignMetaData(montageHandle, montageMetaData);
                                        m_AssetManager->AssignAsset(montageHandle, montage);
                                        m_EditorLayer->SaveProject();

                                        if (DispatchOpenAssetEditorEvent(montageHandle, montageMetaData))
                                        {
                                            Application::SubmitToMainThread([this]() { RefreshFiles(); });
                                        }
                                    }
                                }
                            }
                        }
                        break; // !SkeletalAnimation
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("%s", item.generic_string().c_str());

            ImGui::EndPopup();
        }

        // Move/Copy to directory
        if (isDirectory)
        {
            if (ImGui::BeginDragDropTarget())
            {
                bool queuedPopup = false;

                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                        const AssetMetaData droppedMetadata = m_AssetManager->GetMetaData(droppedHandle);
                        Project *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
                        if (project && droppedHandle != AssetHandle(0) && droppedMetadata.type != AssetType::Invalid && !droppedMetadata.filepath.empty())
                        {
                            QueueMoveCopyPopup(project->GetProjectFilepath(droppedMetadata.filepath), path);
                            queuedPopup = true;
                        }
                    }
                }

                if (!queuedPopup)
                {
                    if (const ImGuiPayload *pathPayload = ImGui::AcceptDragDropPayload(kContentBrowserPathPayload))
                    {
                        if (pathPayload->DataSize == sizeof(ContentBrowserPathPayload))
                        {
                            const auto *dropData = static_cast<const ContentBrowserPathPayload *>(pathPayload->Data);
                            QueueMoveCopyPopup(std::filesystem::path(dropData->path), path);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

        }

        DragDropSource(m_CurrentDirectory / item);

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + maxSize);
        ImGui::Text("%s", item.generic_string().c_str());
        ImGui::PopTextWrapPos();

        // Render sprite sheet contents
        if (!isDirectory && itemAssetType == AssetType::SpriteSheet)
        {
            Project *project = m_EditorLayer->GetActiveProject().get();

            AssetHandle handle = m_AssetManager->GetAssetHandle(path);
            AssetMetaData metadata = m_AssetManager->GetMetaData(handle);

            if (metadata.type == AssetType::SpriteSheet && handle != AssetHandle(0))
            {
                const std::string popupId = std::format("##sprites_popup_{}", item.generic_string());
                if (ImGui::SmallButton(std::format("Sprites##{}", item.generic_string()).c_str()))
                {
                    ImGui::OpenPopup(popupId.c_str());
                }

                if (ImGui::BeginPopup(popupId.c_str()))
                {
                    Ref<SpriteSheet> spriteSheet = m_AssetManager->GetAsset<SpriteSheet>(handle);
                    if (spriteSheet)
                    {
                        Ref<Texture> texture = nullptr;
                        if (spriteSheet->GetTextureHandle() != AssetHandle(0))
                        {
                            texture = m_AssetManager->GetAsset<Texture>(spriteSheet->GetTextureHandle());
                        }

                        const auto &sprites = spriteSheet->GetSprites();
                        if (sprites.empty())
                        {
                            ImGui::TextDisabled("No extracted sprites.");
                        }
                        else
                        {
                            constexpr float spritePreviewSize = 56.0f;
                            for (size_t spriteIndex = 0; spriteIndex < sprites.size(); ++spriteIndex)
                            {
                                const auto &sprite = sprites[spriteIndex];
                                ImGui::PushID(static_cast<int>(spriteIndex));

                                if (texture && texture->GetHandle())
                                {
                                    ImTextureID texId = reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
                                    ImGui::ImageButton("##sprite_item", texId, ImVec2(spritePreviewSize, spritePreviewSize), ImVec2(sprite.uv0.x, sprite.uv0.y), ImVec2(sprite.uv1.x, sprite.uv1.y));
                                }
                                else
                                {
                                    ImGui::Button("N/A", ImVec2(spritePreviewSize, spritePreviewSize));
                                }

                                if (ImGui::BeginDragDropSource())
                                {
                                    SpriteSheetSpritePayload payload;
                                    payload.spriteSheetHandle = handle;
                                    payload.textureHandle = spriteSheet->GetTextureHandle();
                                    payload.spriteIndex = static_cast<uint32_t>(spriteIndex);
                                    payload.uv0 = sprite.uv0;
                                    payload.uv1 = sprite.uv1;

                                    ImGui::SetDragDropPayload(DND_PAYLOAD_SPRITE_SHEET_ITEM, &payload, sizeof(payload));
                                    ImGui::Text("Sprite %zu", spriteIndex);
                                    ImGui::EndDragDropSource();
                                }

                                if ((spriteIndex + 1) % 4 != 0)
                                {
                                    ImGui::SameLine();
                                }

                                ImGui::PopID();
                            }
                        }
                    }
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::EndGroup();
    }

    bool ContentBrowserPanel::DuplicateItem(const std::filesystem::path &filepath)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject() || !m_AssetManager || !std::filesystem::exists(filepath))
        {
            return false;
        }

        if (std::filesystem::is_directory(filepath))
        {
            return false;
        }

        const std::filesystem::path duplicatePath = BuildUniqueSiblingPath(filepath);
        std::error_code ec;
        std::filesystem::copy_file(filepath.string(), duplicatePath.string(), std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            LOG_ERROR("[Content Browser] Failed to duplicate '{}' to '{}': {}", filepath.generic_string(), duplicatePath.generic_string(), ec.message());
            return false;
        }

        // Copy sidecar metadata/settings if present so import settings are preserved.
        const std::filesystem::path sourceMetaPath = filepath.string() + ".meta";
        const std::filesystem::path duplicateMetaPath = duplicatePath.string() + ".meta";
        if (std::filesystem::exists(sourceMetaPath))
        {
            std::error_code metaCopyError;
            std::filesystem::copy_file(sourceMetaPath.string(), duplicateMetaPath.string(), std::filesystem::copy_options::overwrite_existing, metaCopyError);
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        const std::filesystem::path relativeDuplicatePath = project->GetProjectFilepath(duplicatePath);
        const AssetType duplicateType = GetAssetTypeFromExtension(duplicatePath.extension().generic_string());
        if (duplicateType == AssetType::Invalid)
        {
            LOG_ERROR("[Content Browser] Failed to duplicate unsupported asset type: '{}'", duplicatePath.generic_string());
            return false;
        }

        AssetHandle duplicateHandle = AssetHandle();
        AssetMetaData duplicateMetadata;
        duplicateMetadata.type = duplicateType;
        duplicateMetadata.filepath = relativeDuplicatePath;

        m_AssetManager->AssignMetaData(duplicateHandle, duplicateMetadata);

        if (DispatchOpenAssetEditorEvent(duplicateHandle, duplicateMetadata))
        {
            Application::SubmitToMainThread([this]() { RefreshFiles(); });
        }
        return true;
    }

    void ContentBrowserPanel::CreateNewPrefab()
    {
        Project *project = m_EditorLayer->GetActiveProject().get();
        if (!project)
            return;

        std::filesystem::path targetPath = BuildUniqueSiblingPath(m_CurrentDirectory / "NewPrefab.ixprefab");
        Ref<Prefab> prefab = CreateRef<Prefab>(project);

        Entity rootEntity = SceneManager::CreateEntity(prefab->GetPrefabScene().get(), "NewPrefab", EntityType_Node);
        prefab->SetRootEntityUUID(rootEntity.GetUUID());

        if (prefab->Serialize(targetPath))
        {
            m_AssetManager->ImportAsset(targetPath);
            RefreshFiles();
        }
    }

    void ContentBrowserPanel::UpdateSelection(const std::filesystem::path &filepath)
    {
        const bool multiSelect = ImGui::GetIO().KeyCtrl;
        const auto selectedIt = std::ranges::find(m_SelectedItems, filepath);

        if (!multiSelect)
        {
            m_SelectedItems.clear();
            m_SelectedItems.push_back(filepath);
            return;
        }

        if (selectedIt != m_SelectedItems.end())
        {
            m_SelectedItems.erase(selectedIt);
        }
        else
        {
            m_SelectedItems.push_back(filepath);
        }
    }

    std::vector<std::filesystem::path> ContentBrowserPanel::GetDragSourcePaths(const std::filesystem::path &draggedPath) const
    {
        std::vector<std::filesystem::path> result;

        if (!m_SelectedItems.empty() && std::ranges::find(m_SelectedItems, draggedPath) != m_SelectedItems.end())
        {
            result = m_SelectedItems;
        }
        else
        {
            result.push_back(draggedPath);
        }

        std::vector<std::filesystem::path> filtered;
        for (const auto &candidate : result)
        {
            bool nestedInOtherSelection = false;
            for (const auto &other : result)
            {
                if (candidate == other)
                {
                    continue;
                }

                if (IsPathWithin(candidate.string(), other.string()))
                {
                    nestedInOtherSelection = true;
                    break;
                }
            }

            if (!nestedInOtherSelection)
            {
                filtered.push_back(candidate);
            }
        }

        return filtered;
    }

    void ContentBrowserPanel::QueueMoveCopyPopup(const std::filesystem::path &draggedPath, const std::filesystem::path &targetDirectory)
    {
        if (draggedPath.empty() || targetDirectory.empty())
        {
            return;
        }

        const bool draggedPathInActiveSelection = std::ranges::find(m_ActiveDragItems, draggedPath) != m_ActiveDragItems.end();
        std::vector<std::filesystem::path> dragSources = draggedPathInActiveSelection ? m_ActiveDragItems : GetDragSourcePaths(draggedPath);
        std::vector<std::filesystem::path> validSources;
        validSources.reserve(dragSources.size());

        for (const auto &source : dragSources)
        {
            if (!std::filesystem::exists(source))
            {
                continue;
            }

            std::error_code equivalentError;
            const bool samePath = std::filesystem::equivalent(source.string(), targetDirectory.string(), equivalentError);
            if (!equivalentError && samePath)
            {
                continue;
            }

            if (std::filesystem::is_directory(source) && IsPathWithin(targetDirectory.string(), source.string()))
            {
                continue;
            }

            validSources.push_back(source);
        }

        if (validSources.empty())
        {
            return;
        }

        m_PendingDragDropSources = std::move(validSources);
        m_PendingDragDropTargetDirectory = targetDirectory;
        m_ShowMoveCopyPopup = true;
    }

    bool ContentBrowserPanel::MoveOrCopyPathToDirectory(const std::filesystem::path &sourcePath, const std::filesystem::path &targetDirectory, bool moveItem)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject() || !m_AssetManager)
        {
            return false;
        }

        auto project = m_EditorLayer->GetActiveProject();
        if (!project || !std::filesystem::exists(sourcePath) || !std::filesystem::exists(targetDirectory) || !std::filesystem::is_directory(targetDirectory))
        {
            return false;
        }

        // Prevent moving/copying a directory into one of its child directories
        if (std::filesystem::is_directory(sourcePath) && IsPathWithin(targetDirectory.string(), sourcePath.string()))
        {
            LOG_WARN("[Content Browser] Cannot move/copy '{}' into one of its child directories '{}'.", sourcePath.generic_string(), targetDirectory.generic_string());
            return false;
        }

        // Check if the source and target directories are the same
        std::error_code equivalentError;
        const bool sameDirectory = std::filesystem::equivalent(sourcePath.parent_path().string(), targetDirectory.string(), equivalentError);
        if (!equivalentError && sameDirectory && moveItem)
        {
            return false;
        }

        // Determine the destination path and ensure it is unique
        std::filesystem::path destinationPath = targetDirectory / sourcePath.filename();
        if (std::filesystem::exists(destinationPath))
        {
            destinationPath = BuildUniquePathInDirectory(sourcePath, targetDirectory);
        }


        // Perform the move or copy operation
        const bool sourceIsDirectory = std::filesystem::is_directory(sourcePath);
        const std::filesystem::path oldRelativePath = project->GetProjectRelativeFilepath(sourcePath);
        const std::filesystem::path newRelativePath = project->GetProjectRelativeFilepath(destinationPath);
        std::error_code ec;

        // - MOVE
        if (moveItem)
        {
            // Move the source path to the destination path
            std::filesystem::rename(sourcePath.string(), destinationPath.string(), ec);
            if (ec)
            {
                LOG_ERROR("[Content Browser] Failed to move '{}' to '{}': {}", sourcePath.generic_string(), destinationPath.generic_string(), ec.message());
                return false;
            }

            // Move the sidecar metadata/settings if present
            if (sourceIsDirectory)
            {
                auto &registry = m_AssetManager->GetAssetAssetRegistry();
                for (const auto &[assetHandle, metadata] : registry)
                {
                    // Update the metadata for assets that are within the moved directory
                    if (assetHandle == AssetHandle(0) || metadata.filepath.empty() || !IsPathWithin(metadata.filepath.string(), oldRelativePath.string()))
                    {
                        continue;
                    }

                    auto rebasedPath = RebasePath(metadata.filepath.string(), oldRelativePath.string(), newRelativePath.string());
                    LOG_WARN("[Content Browser] AssetHandle {} metadata updated:", static_cast<uint64_t>(assetHandle));
                    LOG_TRACE("[Content Browser]\tOld Filepath: '{}'", metadata.filepath.generic_string());
                    LOG_TRACE("[Content Browser]\tNew Filepath: '{}'", rebasedPath);

                    AssetMetaData updatedMetadata = metadata;
                    updatedMetadata.filepath = rebasedPath;
                    m_AssetManager->AssignMetaData(assetHandle, updatedMetadata);
                }
            }
            else
            {
                // Move the sidecar metadata/settings if present
                const std::filesystem::path sourceMetaPath = sourcePath.string() + ".meta";
                const std::filesystem::path destinationMetaPath = destinationPath.string() + ".meta";
                if (std::filesystem::exists(sourceMetaPath))
                {
                    std::error_code metaError;
                    std::filesystem::rename(sourceMetaPath.string(), destinationMetaPath.string(), metaError);
                }

                const AssetHandle existingHandle = m_AssetManager->GetAssetHandle(oldRelativePath);
                if (existingHandle != AssetHandle(0))
                {
                    AssetMetaData metadata = m_AssetManager->GetMetaData(existingHandle);
                    metadata.filepath = newRelativePath;
                    m_AssetManager->AssignMetaData(existingHandle, metadata);
                }
            }
        }
        // - COPY
        else
        {
            if (sourceIsDirectory)
            {
                std::filesystem::copy(sourcePath.string(), destinationPath.string(), std::filesystem::copy_options::recursive, ec);
            }
            else
            {
                std::filesystem::copy_file(sourcePath.string(), destinationPath.string(), std::filesystem::copy_options::overwrite_existing, ec);
            }

            if (ec)
            {
                LOG_ERROR("[Content Browser] Failed to copy '{}' to '{}': {}", sourcePath.generic_string(), destinationPath.generic_string(), ec.message());
                return false;
            }

            if (!sourceIsDirectory)
            {
                const std::filesystem::path sourceMetaPath = sourcePath.string() + ".meta";
                const std::filesystem::path destinationMetaPath = destinationPath.string() + ".meta";
                if (std::filesystem::exists(sourceMetaPath))
                {
                    std::error_code metaError;
                    std::filesystem::copy_file(sourceMetaPath.string(), destinationMetaPath.string(), std::filesystem::copy_options::overwrite_existing, metaError);
                }
            }

            auto &registry = m_AssetManager->GetAssetAssetRegistry();
            for (const auto &[assetHandle, metadata] : registry)
            {
                if (assetHandle == AssetHandle(0) || metadata.filepath.empty() || !IsPathWithin(metadata.filepath.string(), oldRelativePath.string()))
                {
                    continue;
                }

                AssetHandle copiedHandle = AssetHandle();
                AssetMetaData copiedMetadata = metadata;
                copiedMetadata.filepath = RebasePath(metadata.filepath.string(), oldRelativePath.string(), newRelativePath.string());
                m_AssetManager->AssignMetaData(copiedHandle, copiedMetadata);
            }
        }

        return true;
    }

    bool ContentBrowserPanel::MoveOrCopySelectionToDirectory(const std::filesystem::path &targetDirectory, bool moveItem)
    {
        if (m_PendingDragDropSources.empty() || targetDirectory.empty())
        {
            return false;
        }

        bool changed = false;
        for (const auto &sourcePath : m_PendingDragDropSources)
        {
            changed |= MoveOrCopyPathToDirectory(sourcePath, targetDirectory, moveItem);
        }

        if (changed)
        {
            m_EditorLayer->SaveProject();
            Application::SubmitToMainThread([this]() { RefreshFiles(); });
        }

        return changed;
    }

    void ContentBrowserPanel::UIRenderNavigationBar()
    {
        IGN_PROFILE_SCOPE_COLOR("ContentBrowser::UIRenderNavigationBar", 0xCD5C5C);

        const ImGuiStyle &style = ImGui::GetStyle();

        const auto navbarBtSize = ImVec2(38.0f, 32.0f);
        // const float navbarHeight = navbarBtSize.y + style.FramePadding.y * 2.0f + style.WindowPadding.y * 2.0f;
        if (ImGui::BeginChild("##NAV_BUTTON_BAR", ImVec2(0, navbarBtSize.y), ImGuiChildFlags_Borders))
        {
            ImTextureID arrow = (ImTextureID)s_SharedIcons["arrow"]->GetHandle().Get();
            if (UI::DrawImageButton("##bw_arrow", arrow, navbarBtSize))
            {
                if (!m_BackwardPathStack.empty())
                {
                    const std::filesystem::path previousPath = m_BackwardPathStack.top();
                    m_BackwardPathStack.pop();

                    if (previousPath != m_CurrentDirectory)
                    {
                        if (m_ForwardPathStack.empty() || m_ForwardPathStack.top() != m_CurrentDirectory)
                        {
                            m_ForwardPathStack.push(m_CurrentDirectory);
                        }
                        m_CurrentDirectory = previousPath;
                    }
                }
            }

            ImGui::SameLine(0.0f, 4.0f);
            if (UI::DrawImageButton("##fw_arrow", arrow, navbarBtSize, {1.0f, 0.0f}, { 0.0f, 1.0f }))
            {
                if (!m_ForwardPathStack.empty())
                {
                    const std::filesystem::path nextPath = m_ForwardPathStack.top();
                    m_ForwardPathStack.pop();

                    if (nextPath != m_CurrentDirectory)
                    {
                        if (m_BackwardPathStack.empty() || m_BackwardPathStack.top() != m_CurrentDirectory)
                        {
                            m_BackwardPathStack.push(m_CurrentDirectory);
                        }
                        m_CurrentDirectory = nextPath;
                    }
                }
            }

            ImGui::SameLine(0.0f, 4.0f);
            ImTextureID refreshBt = (ImTextureID)s_SharedIcons["roll"]->GetHandle().Get();
            if (UI::DrawImageButton("##refresh_bt", refreshBt, { navbarBtSize.y, navbarBtSize.y }))
            {
                m_EditorLayer->GetActiveProject()->ValidateAssetRegistry();
                PruneMissingNodes(0, m_EditorLayer->GetActiveProject()->GetDirectory());
                RefreshAssetTree();
                CompactTree();
            }
        }
        ImGui::EndChild();
    }

    void ContentBrowserPanel::PruneMissingNodes(uint32_t nodeIndex, const std::filesystem::path &basePath)
    {
        IGN_PROFILE_FUNCTION();

        if (nodeIndex >= m_TreeNodes.size() || m_TreeNodes[nodeIndex].isDeleted)
            return;

        FileTreeNode &node = m_TreeNodes[nodeIndex];
        std::vector<std::string> toRemove;

        for (auto it = node.children.begin(); it != node.children.end(); )
        {
            auto &[childName, childIndex] = *it;

            if (childIndex >= m_TreeNodes.size() || m_TreeNodes[childIndex].isDeleted)
            {
                it = node.children.erase(it);
                continue;
            }

            std::filesystem::path fullPath = basePath / GetNodeFullpath(childIndex);

            if (!std::filesystem::exists(fullPath))
            {
                toRemove.push_back((childName.string()));
            }
            else if (std::filesystem::is_directory(fullPath))
            {
                PruneMissingNodes(childIndex, fullPath);
            }
            ++it;
        }

        for (const auto &name : toRemove)
        {
            if (auto it = node.children.find(name); it != node.children.end())
            {
                const uint32_t childIndex = it->second;
                MarkNodeDeletedRecursive(childIndex);
                node.children.erase(it);
            }
        }
    }

    void ContentBrowserPanel::PruneMissingNodesAlt(uint32_t nodeIndex, const std::filesystem::path &basePath)
    {
        IGN_PROFILE_FUNCTION();

        std::vector<uint32_t> nodesToDelete;
        CollectNodesToDelete(nodeIndex, basePath, nodesToDelete);

        // sort in descending order to delete from highest index first
        std::ranges::sort(nodesToDelete.rbegin(), nodesToDelete.rend());
        for (uint32_t nodeToDelete : nodesToDelete)
        {
            DeleteSingleNode(nodeToDelete);
        }
    }

    void ContentBrowserPanel::CollectNodesToDelete(uint32_t nodeIndex, const std::filesystem::path &basePath, std::vector<uint32_t> &nodesToDelete)
    {
        IGN_PROFILE_FUNCTION();

        FileTreeNode &node = m_TreeNodes[nodeIndex];

        for (auto &childIndex : node.children | std::views::values)
        {
            std::filesystem::path fullPath = basePath / GetNodeFullpath(childIndex);
            if (!std::filesystem::exists(fullPath))
            {
                CollectNodeAndDescendants(childIndex, nodesToDelete);
            }
            else if (std::filesystem::is_directory(fullPath))
            {
                CollectNodesToDelete(childIndex, fullPath, nodesToDelete);
            }
        }
    }

    void ContentBrowserPanel::CollectNodeAndDescendants(uint32_t nodeIndex, std::vector<uint32_t> &nodesToDelete)
    {
        IGN_PROFILE_FUNCTION();

        FileTreeNode &node = m_TreeNodes[nodeIndex];

        for (auto &childIndex : node.children | std::views::values)
        {
            CollectNodeAndDescendants(childIndex, nodesToDelete);
        }

        nodesToDelete.push_back(nodeIndex);
    }

    void ContentBrowserPanel::MarkNodeDeletedRecursive(uint32_t nodeIndex)
    {
        IGN_PROFILE_FUNCTION();

        if (nodeIndex >= m_TreeNodes.size() || m_TreeNodes[nodeIndex].isDeleted)
            return;

        FileTreeNode &node = m_TreeNodes[nodeIndex];
        node.isDeleted = true;

        for (auto &childIndex : node.children | std::views::values)
        {
            MarkNodeDeletedRecursive(childIndex);
        }

        node.children.clear();
    }

    void ContentBrowserPanel::DeleteSingleNode(uint32_t nodeIndex)
    {
        IGN_PROFILE_FUNCTION();

        // Remove this node from its parent's children map
        if (nodeIndex < m_TreeNodes.size())
        {
            FileTreeNode &nodeToDelete = m_TreeNodes[nodeIndex];
            if (nodeToDelete.parent != static_cast<uint32_t>(-1) && nodeToDelete.parent < m_TreeNodes.size())
            {
                FileTreeNode &parent = m_TreeNodes[nodeToDelete.parent];
                for (auto it = parent.children.begin(); it != parent.children.end(); ++it)
                {
                    if (it->second == nodeIndex)
                    {
                        parent.children.erase(it);
                        break;
                    }
                }
            }
        }

        // Erase the node
        m_TreeNodes.erase(m_TreeNodes.begin() + nodeIndex);

        // Update all indices greater than nodeIndex
        UpdateIndicesAfterDeletion(nodeIndex);
    }

    void ContentBrowserPanel::UpdateIndicesAfterDeletion(uint32_t deletedIndex)
    {
        IGN_PROFILE_FUNCTION();

        for (auto &node : m_TreeNodes)
        {
            // Update parent index
            if (node.parent > deletedIndex)
            {
                node.parent--;
            }

            // Update children indices
            for (auto &childIndex : node.children | std::views::values)
            {
                if (childIndex > deletedIndex)
                {
                    childIndex--;
                }
            }
        }
    }

    void ContentBrowserPanel::CompactTree()
    {
        IGN_PROFILE_FUNCTION();

        std::vector<FileTreeNode> newNodes;
        std::unordered_map<uint32_t, uint32_t> indexMapping;

        // First pass: copy non-deleted nodes and create index mapping
        for (uint32_t i = 0; i < m_TreeNodes.size(); ++i)
        {
            if (!m_TreeNodes[i].isDeleted)
            {
                indexMapping[i] = static_cast<uint32_t>(newNodes.size());
                newNodes.push_back(m_TreeNodes[i]);
            }
        }

        // Second pass: update all indices
        for (auto &node : newNodes)
        {
            // Update parent index
            if (node.parent != 0)
            {
                auto it = indexMapping.find(node.parent);
                node.parent = (it != indexMapping.end()) ? it->second : 0;
            }

            // Update children indices
            for (auto &childIndex : node.children | std::views::values)
            {
                if (auto it = indexMapping.find(childIndex); it != indexMapping.end())
                {
                    childIndex = it->second;
                }
            }

            // Remove children that were deleted
            for (auto it = node.children.begin(); it != node.children.end();)
            {
                if (!indexMapping.contains(it->second))
                {
                    //it = node.children.erase(it);
                    ++it;
                }
                else
                {
                    ++it;
                }
            }
        }

        m_TreeNodes = std::move(newNodes);
        RebuildSortedTreeCache();
    }

    void ContentBrowserPanel::DragDropSource(const std::filesystem::path &filepath)
    {
        if (ImGui::BeginDragDropSource())
        {
            IGN_PROFILE_SCOPE("ContentBrowser::DragDropSource");

            m_ActiveDragItems = GetDragSourcePaths(filepath);
            auto project = m_EditorLayer->GetActiveProject();
            const std::filesystem::path relativeAssetPath = project ? project->GetProjectFilepath(filepath) : filepath;

            const bool isDirectory = std::filesystem::is_directory(filepath);
            if (!isDirectory)
            {
                AssetHandle handle = project ? m_AssetManager->GetAssetHandle(relativeAssetPath) : AssetHandle(0);
                if (handle != AssetHandle(0))
                {
                    ImGui::SetDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM, &handle, sizeof(AssetHandle));
                }
            }
            else
            {
                ContentBrowserPathPayload payload;
                const std::string pathString = filepath.generic_string();
                std::strncpy(payload.path, pathString.c_str(), sizeof(payload.path) - 1);
                ImGui::SetDragDropPayload(kContentBrowserPathPayload, &payload, sizeof(payload));
            }

            Ref<Texture> dragIcon = GetOrCreateThumbnail(filepath, isDirectory);
            if (!dragIcon)
            {
                dragIcon = s_SharedIcons["unknown"];
            }

            if (dragIcon && dragIcon->GetHandle())
            {
                ImTextureID iconId = reinterpret_cast<ImTextureID>(dragIcon->GetHandle().Get());
                ImGui::Image(iconId, ImVec2(32.0f, 32.0f));
            }

            if (m_ActiveDragItems.size() > 1)
            {
                ImGui::Text("%zu items", m_ActiveDragItems.size());
            }
            else
            {
                ImGui::Text("%s", filepath.filename().string().c_str());
            }

            ImGui::EndDragDropSource();
        }
    }

    void ContentBrowserPanel::OnImportAssetDialog(void *userData, const char *const *filelist, int filter)
    {
        IGN_PROFILE_FUNCTION();

        ContentBrowserPanel *data = (ContentBrowserPanel *)userData;
        if (!data)
        {
            LOG_ERROR("Import Asset Dialog: Content browser data");
            return;
        }

        if (filelist == nullptr)
        {
            const char *error = SDL_GetError();
            LOG_ERROR("Import Asset Dialog Error: {0}", error ? error : "Unknown error");
            return;
        }

        std::vector<FileImportPayload> filePayloads;
        for (const char *const *file = filelist; *file != nullptr; file++)
        {
            FileImportPayload payload;
            payload.status = FileStatus::Pending;
            payload.type = ImportType::Import;
            payload.metadata.filepath = std::string(*file);
            payload.metadata.type = GetAssetTypeFromExtension(std::filesystem::path(*file).extension().string());
            filePayloads.emplace_back(payload);
        }

        Application::SubmitToMainThread([filePayloads, data]()
        {
            SignalBus::Emit(AssetImportSignal{
                .payloads = std::move(filePayloads),
                .targetDirectory = data->m_CurrentDirectory }
            );
        });
    }

    std::filesystem::path ContentBrowserPanel::GetNodeFullpath(uint32_t nodeIndex) const
    {
        if (nodeIndex == 0 || nodeIndex >= m_TreeNodes.size())
            return std::filesystem::path();

        std::filesystem::path result;
        uint32_t currentIndex = nodeIndex;

        // Build path from leaf to root
        while (currentIndex != 0 && currentIndex < m_TreeNodes.size())
        {
            const FileTreeNode &node = m_TreeNodes[currentIndex];
            if (node.isDeleted)
                continue;

            // Avoid using operator/ with empty path to prevent trailing separators
            if (result.empty())
                result = node.path;
            else
                result = node.path / result;

            currentIndex = node.parent;
        }

        return result;
    }

    bool ContentBrowserPanel::IsImageFile(const std::filesystem::path &filepath) const
    {
        IGN_PROFILE_FUNCTION();

        if (!std::filesystem::exists(filepath))
            return false;

        std::string ext = filepath.extension().string();
        return GetAssetTypeFromExtension(ext) == AssetType::Texture;
    }

    ImVec2 ContentBrowserPanel::CalculateThumbnailDisplaySize(Ref<Texture> texture, float maxSize) const
    {
        IGN_PROFILE_FUNCTION();

        if (!texture)
        {
            return ImVec2(maxSize, maxSize);
        }

        float textureWidth = static_cast<float>(texture->GetWidth());
        float textureHeight = static_cast<float>(texture->GetHeight());

        if (textureWidth <= 0 || textureHeight <= 0)
        {
            return ImVec2(maxSize, maxSize);
        }

        float aspectRatio = textureWidth / textureHeight;
        ImVec2 displaySize;

        if (aspectRatio > 1.0f)
        {
            // Wider than tall
            displaySize.x = maxSize;
            displaySize.y = maxSize / aspectRatio;
        }
        else
        {
            // Taller than wide
            displaySize.x = maxSize * aspectRatio;
            displaySize.y = maxSize;
        }

        return displaySize;
    }

    Ref<Texture> ContentBrowserPanel::GetOrCreateThumbnail(const std::filesystem::path &filepath, bool isDirectory)
    {
        IGN_PROFILE_FUNCTION();

        if (isDirectory)
            return s_SharedIcons["folder"];

        AssetType type = GetAssetTypeFromExtension(filepath.extension().string());
        switch (type)
        {
            case AssetType::Scene: return s_SharedIcons["scene"];
            case AssetType::Audio: return s_SharedIcons["audio"];
            case AssetType::Shader: return s_SharedIcons["shader"];
            case AssetType::SpriteSheet: return s_SharedIcons["sprite_sheet"];
            case AssetType::Material: return s_SharedIcons["material"];
            case AssetType::Material2D: return s_SharedIcons["material_2d"];
            case AssetType::SkeletalAnimation: return s_SharedIcons["anim"];
            case AssetType::Font: return s_SharedIcons["font"];
            case AssetType::Skeleton: return s_SharedIcons["skeleton"];
            case AssetType::Mesh: return s_SharedIcons["mesh"];
            case AssetType::SkeletalMesh: return s_SharedIcons["mesh"];
            case AssetType::StaticMesh: return s_SharedIcons["mesh"];
            case AssetType::Animation2D: return s_SharedIcons["anim_2d"];
            case AssetType::AnimatorController: return s_SharedIcons["anim_ctrl"];
            case AssetType::AnimatorController2D: return s_SharedIcons["anim_ctrl_2d"];
            case AssetType::Terrain: return s_SharedIcons["mesh"];
            default: break;
        }

        if (filepath.extension() == ".cs")
            return s_SharedIcons["font"]; // reuse font icon or we could add a script icon

        auto it = s_SharedThumbnails.find(filepath);
        if (it != s_SharedThumbnails.end())
        {
            // Mark as used this frame
            it->second.lastFrameUsed = s_SharedCurrentFrame;

            if (!it->second.thumbnail && !s_SharedThumbnailLoadsInFlight.contains(filepath))
            {
                s_SharedPendingThumbnailLoads.push(filepath);
                s_SharedThumbnailLoadsInFlight.insert(filepath);
            }

            return it->second.thumbnail;
        }

        // Generate image only
        if (IsImageFile(filepath))
        {
            // Create placeholder entry to prevent duplicate jobs
            FileThumbnail placeholder;
            placeholder.thumbnail = nullptr;
            placeholder.timestamp = 0;
            placeholder.lastFrameUsed = s_SharedCurrentFrame;
            s_SharedThumbnails[filepath] = placeholder;

            // Add to loading queue instead of starting immediately
            if (!s_SharedThumbnailLoadsInFlight.contains(filepath))
            {
                s_SharedPendingThumbnailLoads.push(filepath);
                s_SharedThumbnailLoadsInFlight.insert(filepath);
            }
        }

        return s_SharedIcons["unknown"];
    }

    void ContentBrowserPanel::StartThumbnailLoad(const std::filesystem::path &filepath)
    {
        IGN_PROFILE_FUNCTION();

        // Capture by value to avoid dangling references
        std::filesystem::path capturedPath = filepath;
        int thumbnailSize = m_ThumbnailSize;
        const uint64_t requestGeneration = s_SharedThumbnailLoadGeneration;

        AssetWorker::SubmitJob("Generating thumbnails...", [this, capturedPath, thumbnailSize, requestGeneration]()
        {
            // -----------------------------------------------------------------------
            // WORKER THREAD: Load raw CPU pixels only — NO GPU objects created here.
            // -----------------------------------------------------------------------
            if (requestGeneration != s_SharedThumbnailLoadGeneration)
            {
                Application::SubmitToRenderThread([this, capturedPath]()
                {
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                    s_SharedThumbnails.erase(capturedPath);
                }, "ContentBrowserPanel::StartThumbnailLoad - Request submit thumbnail");
                return;
            }

            constexpr int kChannels = 4;
            int srcWidth = 0, srcHeight = 0, channelsOut = 0;

            // stbi_load converts HDR/float images to 8-bit, which is fine for a small
            // thumbnail preview and avoids the float-to-GPU-memory path entirely.
            std::vector<uint8_t> srcPixels;
            std::vector<uint8_t> destPixels;
            const int destWidth = thumbnailSize;
            const int destHeight = thumbnailSize;
            if (texture_utils::IsExrFile(capturedPath))
            {
                nvrhi::Format format = nvrhi::Format::RGBA8_UNORM;
                texture_utils::LoadEXRTexture(capturedPath, srcWidth, srcHeight, format, srcPixels);

                const uint64_t resizedSize = static_cast<uint64_t>(destWidth) * destHeight * kChannels;
                destPixels.resize(resizedSize);
                texture_utils::DownSample(srcPixels.data(), destPixels.data(), srcWidth, srcHeight, destWidth, destHeight, kChannels);
            }
            else
            {
                uint8_t *pixelData = stbi_load(capturedPath.string().c_str(), &srcWidth, &srcHeight, &channelsOut, kChannels);
                size_t pixelSize = srcWidth * srcHeight * kChannels;

                srcPixels.resize(pixelSize);
                std::memcpy(srcPixels.data(), pixelData, pixelSize);

                if (!pixelData || srcWidth <= 0 || srcHeight <= 0)
                {
                    Application::SubmitToRenderThread([this, capturedPath]()
                    {
                        s_SharedThumbnails.erase(capturedPath);
                        s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                    }, "ContentBrowserPanel::StartThumbnailLoad - Delete pixels");
                    return;
                }

                // -----------------------------------------------------------------------
                // KEY FIX: Downsample to thumbnail size on the CPU *before* uploading.
                //
                // The original code called Texture::Create(filepath, createInfo, nullptr)
                // which ignores createInfo.width/height and loads the full resolution image
                // (e.g. an 8K x 4K HDR = 128MB GPU allocation just for a 96px thumbnail).
                // With many HDR files, this easily causes ~300-600 MB growth per reload cycle
                // and the unload only freed the Ref but nvrhi deferred deletion kept them
                // alive long enough for the next reload to stack on top.
                // -----------------------------------------------------------------------
                const uint64_t resizedSize = static_cast<uint64_t>(destWidth) * destHeight * kChannels;
                destPixels.resize(resizedSize);
                texture_utils::DownSample(srcPixels.data(), destPixels.data(), srcWidth, srcHeight, destWidth, destHeight, kChannels);
                stbi_image_free(pixelData);
                pixelData = nullptr;
            }

            // -----------------------------------------------------------------------
            // RENDER THREAD: Create GPU texture from the tiny downsampled buffer.
            // GPU object creation happens here — never on a worker thread.
            // -----------------------------------------------------------------------
            Application::SubmitToRenderThread([this, capturedPath, destPixels = destPixels, destWidth, destHeight, requestGeneration]() mutable
            {
                // Drop early if canceled
                if (requestGeneration != s_SharedThumbnailLoadGeneration)
                {
                    destPixels.clear();
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                    s_SharedThumbnails.erase(capturedPath);
                    return;
                }

                auto thumbnailIt = s_SharedThumbnails.find(capturedPath);
                if (thumbnailIt == s_SharedThumbnails.end() || !destPixels.data())
                {
                    destPixels.clear();
                    s_SharedThumbnails.erase(capturedPath);
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                    return;
                }

                TextureCreateInfo createInfo;
                createInfo.format = nvrhi::Format::RGBA8_UNORM;
                createInfo.keepInitialState = true;
                createInfo.keepCpuData = false;
                createInfo.deferGpuCreate = true;
                createInfo.bindless = false;
                createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
                createInfo.width = static_cast<uint32_t>(destWidth);
                createInfo.height = static_cast<uint32_t>(destHeight);
                createInfo.samplerAddressU = nvrhi::SamplerAddressMode::ClampToEdge;
                createInfo.samplerAddressV = nvrhi::SamplerAddressMode::ClampToEdge;
                createInfo.samplerAddressW = nvrhi::SamplerAddressMode::ClampToEdge;
                createInfo.samplerLinearFiltering = true;

                nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                nvrhi::CommandListHandle cmd = device->createCommandList();

                // Build the texture from the pre-resized buffer (Buffer overload).
                // createInfo.width/height ARE honoured by this constructor.
                Ref<Texture> loadedTexture = Texture::Create(destPixels, createInfo, nullptr);

                if (!loadedTexture)
                {
                    s_SharedThumbnails.erase(capturedPath);
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                    return;
                }

                {
                    std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
                    cmd->open();
                    cmd->beginMarker("Content browser thumbnails creation");
                    loadedTexture->SetData(cmd, kChannels);
                    cmd->endMarker();
                    cmd->close();
                }

                Application::SubmitWorkerCommandList(cmd, [this, loadedTexture, capturedPath, requestGeneration]()
                {
                    // Drop the texture if the generation changed or the entry was evicted
                    // while the command list was queued — do NOT re-insert it.
                    if (requestGeneration != s_SharedThumbnailLoadGeneration)
                    {
                        s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                        return;
                    }

                    auto thumbnailIt = s_SharedThumbnails.find(capturedPath);
                    if (thumbnailIt == s_SharedThumbnails.end())
                    {
                        s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                        return;
                    }

                    loadedTexture->SetReadyFlag(true);

                    FileThumbnail ft;
                    ft.thumbnail = loadedTexture;
                    ft.lastFrameUsed = s_SharedCurrentFrame;
                    ft.timestamp = std::filesystem::exists(capturedPath)
                        ? std::filesystem::last_write_time(capturedPath.string()).time_since_epoch().count()
                        : 0;

                    // Overwrite the placeholder that was inserted before the load started.
                    thumbnailIt->second = ft;
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                });
            }, "ContentBrowserPanel::StartThumbnailLoad - Create texture");
        });
    }

    void ContentBrowserPanel::UnloadUnusedThumbnails()
    {
        IGN_PROFILE_FUNCTION();

        std::vector<std::filesystem::path> toUnload;

        for (const auto& [path, thumbnail] : s_SharedThumbnails)
        {
            const bool isStale = (s_SharedCurrentFrame - thumbnail.lastFrameUsed) > s_ThumbnailUnloadFrameThreshold;

            // Unload stale GPU thumbnails and also stale placeholders to prevent map growth.
            // Keep placeholders that are currently loading to avoid duplicate in-flight reloads.
            if (isStale && (thumbnail.thumbnail || !s_SharedThumbnailLoadsInFlight.contains(path)))
            {
                toUnload.push_back(path);
            }
        }

        // Unload the thumbnails — erase from the map FIRST so that any in-flight
        // SubmitWorkerCommandList callbacks that fire after this point will find no
        // entry in s_SharedThumbnails and will safely drop their loadedTexture Ref
        // instead of re-inserting a freshly uploaded texture into an evicted slot.
        for (const auto& path : toUnload)
        {
            // Cancel in-flight loads for this path so the callback knows to drop the texture.
            s_SharedThumbnailLoadsInFlight.erase(path);
            // Erase the map entry. Dropping the Ref<Texture> here releases the CPU buffer
            // and allows the GPU resource ref-count to fall to zero once the GPU is done.
            s_SharedThumbnails.erase(path);
        }
    }

    void ContentBrowserPanel::ClearThumbnails()
    {
        s_SharedThumbnailLoadGeneration++;
        s_SharedThumbnails.clear();
        s_SharedThumbnailLoadsInFlight.clear();

        // Clear the pending load queue
        while (!s_SharedPendingThumbnailLoads.empty())
        {
            s_SharedPendingThumbnailLoads.pop();
        }
    }
}
