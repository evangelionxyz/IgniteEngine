
//Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "content_browser_panel.hpp"
#include "ignite/project/project.hpp"
#include "editor_layer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/thumbnail_generator.hpp"
#include "ignite/graphics/objects/mesh.hpp"
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
#include <cmath>
#include <mutex>
#include <string_view>
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
    SignalToken ContentBrowserPanel::s_AssetChangeSignalToken = kInvalidSignalToken;

    namespace
    {
        static constexpr const char *kContentBrowserPathPayload = "content_browser_path";

        struct ContentBrowserPathPayload
        {
            char path[1024] = {};
        };

        struct FileVisual
        {
            const char *icon = ICON_LC_FILE;
            glm::vec4 color = UI::GetTheme().mutedForeground;
            const char *typeBadge = "Asset";
        };

        static FileVisual GetFileVisual(bool isDirectory, bool isOpen, AssetType type, const std::filesystem::path &filepath)
        {
            const auto &t = UI::GetTheme();
            if (isDirectory)
            {
                return { isOpen ? ICON_LC_FOLDER_OPEN : ICON_LC_FOLDER, UI::Accent::Folder, "Folder" };
            }

            const std::string ext = stringutils::ToLower(filepath.extension().string());
            if (ext == ".cs")
            {
                return { ICON_LC_FILE_CODE_2, UI::Accent::ScriptCS, "C# Script" };
            }
            if (ext == ".md" || ext == ".txt" || ext == ".json")
            {
                return { ICON_LC_FILE_TEXT, t.mutedForeground, "Document" };
            }

            switch (type)
            {
                case AssetType::Scene:                return { ICON_LC_LAYERS, UI::Accent::Environment, "Scene" };
                case AssetType::Prefab:               return { ICON_LC_PACKAGE, UI::Accent::Mesh, "Prefab" };
                case AssetType::StaticMesh:
                case AssetType::Mesh:                 return { ICON_LC_BOX, UI::Accent::Mesh, "Static Mesh" };
                case AssetType::SkeletalMesh:         return { ICON_LC_BONE, UI::Accent::Animation, "Skeletal Mesh" };
                case AssetType::Skeleton:             return { ICON_LC_GIT_FORK, UI::Accent::Animation, "Skeleton" };
                case AssetType::Material:             return { ICON_LC_PALETTE, UI::Accent::Material, "Material" };
                case AssetType::Material2D:           return { ICON_LC_PAINTBRUSH, UI::Accent::Material, "Material 2D" };
                case AssetType::Texture:              return { ICON_LC_IMAGE, UI::Accent::Camera, "Texture" };
                case AssetType::Audio:                return { ICON_LC_VOLUME_2, UI::Accent::Audio, "Audio" };
                case AssetType::LuaScript:            return { ICON_LC_FILE_CODE, UI::Accent::ScriptLua, "Lua Script" };
                case AssetType::Shader:               return { ICON_LC_SPARKLES, UI::Accent::Environment, "Shader" };
                case AssetType::SpriteSheet:          return { ICON_LC_GRID_2X2, UI::Accent::Material, "Sprite Sheet" };
                case AssetType::Widget:               return { ICON_LC_LAYOUT_TEMPLATE, UI::Accent::Widget, "Widget" };
                case AssetType::SkeletalAnimation:
                case AssetType::Animation2D:          return { ICON_LC_FILM, UI::Accent::Animation, "Animation" };
                case AssetType::AnimatorController:
                case AssetType::AnimatorController2D: return { ICON_LC_CLAPPERBOARD, UI::Accent::Animation, "Animator" };
                case AssetType::BlendSpace:           return { ICON_LC_SLIDERS_HORIZONTAL, UI::Accent::Animation, "Blend Space" };
                case AssetType::AnimationMontage:     return { ICON_LC_LAYERS_2, UI::Accent::Animation, "Montage" };
                case AssetType::Terrain:              return { ICON_LC_MOUNTAIN, UI::Accent::Environment, "Terrain" };
                case AssetType::Font:                 return { ICON_LC_TYPE, t.foreground, "Font" };
                case AssetType::ScriptableObject:     return { ICON_LC_BRACES, UI::Accent::ScriptCS, "ScriptableObj" };
                default: break;
            }

            return { ICON_LC_FILE, t.mutedForeground, "File" };
        }

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

        if (s_AssetChangeSignalToken == kInvalidSignalToken)
        {
            s_AssetChangeSignalToken = SignalBus::Subscribe<AssetChangeSignal>(
                [](const AssetChangeSignal &signal)
                {
                    InvalidateThumbnailForAsset(signal);
                });
        }

        if (!s_SharedIcons.empty())
        {
            return;
        }

        TextureCreateInfo createInfo;
        createInfo.format = nri::Format::RGBA8_UNORM;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.initialAccess = nri::AccessBits::SHADER_RESOURCE;
        createInfo.keepInitialState = true;
        createInfo.bindless = false;

        s_SharedIcons["scene"] = Texture::Create("resources/ui/editor/ic_editor_scene.png", createInfo, nullptr);
        s_SharedIcons["shader"] = Texture::Create("resources/ui/editor/ic_editor_shader.png", createInfo, nullptr);
        s_SharedIcons["sprite_sheet"] = Texture::Create("resources/ui/editor/ic_editor_sprite_sheet.png", createInfo, nullptr);
        s_SharedIcons["material"] = Texture::Create("resources/ui/editor/ic_editor_material.png", createInfo, nullptr);
        s_SharedIcons["material_2d"] = Texture::Create("resources/ui/editor/ic_editor_material_2d.png", createInfo, nullptr);
        s_SharedIcons["anim"] = Texture::Create("resources/ui/editor/ic_editor_anim.png", createInfo, nullptr);
        s_SharedIcons["font"] = Texture::Create("resources/ui/editor/ic_editor_font.png", createInfo, nullptr);
        s_SharedIcons["roll"] = Texture::Create("resources/ui/editor/ic_editor_roll.png", createInfo, nullptr);
        s_SharedIcons["arrow"] = Texture::Create("resources/ui/editor/ic_editor_arrow.png", createInfo, nullptr);
        s_SharedIcons["add"] = Texture::Create("resources/ui/editor/ic_editor_add.png", createInfo, nullptr);
        s_SharedIcons["audio"] = Texture::Create("resources/ui/editor/ic_editor_audio.png", createInfo, nullptr);
        s_SharedIcons["csharp"] = Texture::Create("resources/ui/editor/ic_csharp.png", createInfo, nullptr);
        s_SharedIcons["lua"] = Texture::Create("resources/ui/editor/ic_lua.png", createInfo, nullptr);

        s_SharedIcons["skeleton"] = Texture::Create("resources/ui/editor/ic_editor_skeleton.png", createInfo, nullptr);
        s_SharedIcons["mesh"] = Texture::Create("resources/ui/editor/ic_editor_sk_mesh.png", createInfo, nullptr);
        s_SharedIcons["st_mesh"] = Texture::Create("resources/ui/editor/ic_editor_st_mesh.png", createInfo, nullptr);

        s_SharedIcons["anim_ctrl"] = Texture::Create("resources/ui/editor/ic_editor_anim_ctrl.png", createInfo, nullptr);
        s_SharedIcons["anim_2d"] = Texture::Create("resources/ui/editor/ic_editor_anim_2d.png", createInfo, nullptr);
        s_SharedIcons["anim_ctrl_2d"] = Texture::Create("resources/ui/editor/ic_editor_anim_ctrl_2d.png", createInfo, nullptr);

        s_SharedIcons["folder"] = Texture::Create("resources/ui/editor/ic_editor_folder.png", createInfo, nullptr);
        s_SharedIcons["unknown"] = Texture::Create("resources/ui/editor/ic_editor_unknown.png", createInfo, nullptr);
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
        if (s_AssetChangeSignalToken != kInvalidSignalToken)
        {
            SignalBus::Unsubscribe<AssetChangeSignal>(s_AssetChangeSignalToken);
            s_AssetChangeSignalToken = kInvalidSignalToken;
        }

        ThumbnailGenerator::Get().Shutdown();

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

    bool ContentBrowserPanel::RevealAsset(AssetHandle handle)
    {
        if (handle == AssetHandle(0) || !m_EditorLayer)
        {
            return false;
        }

        AssetManager *am = m_AssetManager ? m_AssetManager : AssetManager::GetInstance();
        Ref<Project> project = m_EditorLayer->GetActiveProject();
        if (!am || !project)
        {
            return false;
        }

        const AssetMetaData metadata = am->GetMetaData(handle);
        if (metadata.type == AssetType::Invalid || metadata.filepath.empty())
        {
            return false;
        }

        const std::filesystem::path fullPath = metadata.filepath.is_absolute()
            ? metadata.filepath
            : project->GetProjectFilepath(metadata.filepath);
        return RevealPath(fullPath);
    }

    bool ContentBrowserPanel::RevealPath(const std::filesystem::path &fullPath)
    {
        if (fullPath.empty())
        {
            return false;
        }

        const std::filesystem::path targetPath = fullPath.lexically_normal();
        std::error_code ec;
        if (!std::filesystem::exists(targetPath, ec))
        {
            return false;
        }

        const bool isDir = std::filesystem::is_directory(targetPath, ec);
        const std::filesystem::path targetDir = (isDir ? targetPath : targetPath.parent_path()).lexically_normal();
        if (targetDir.empty())
        {
            return false;
        }

        if (targetDir != m_CurrentDirectory)
        {
            if (!m_CurrentDirectory.empty())
            {
                m_BackwardPathStack.push(m_CurrentDirectory);
            }
            while (!m_ForwardPathStack.empty())
            {
                m_ForwardPathStack.pop();
            }
            m_CurrentDirectory = targetDir;
        }

        m_SelectedFileTree = targetPath;
        m_SelectedItems = { targetPath };
        m_PendingScrollToPath = targetPath;
        m_PendingFocusWindow = true;
        m_ClearSearchRequested = true;
        return true;
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

        if (!node || node->path.empty() || node->fullPath.empty())
            return;

        const std::filesystem::path &fullPath = node->fullPath;
        const std::string &filename = node->filename;
        const bool isDirectory = node->isDirectory;

        ImGuiTreeNodeFlags flags = (m_SelectedFileTree == fullPath ? ImGuiTreeNodeFlags_Selected : 0)
            | ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_SpanFullWidth;

        if (!isDirectory)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool isCurrentDir = (fullPath == m_CurrentDirectory);
        const float rowCursorX = ImGui::GetCursorScreenPos().x;
        const std::string displayLabel = "     " + filename;
        const bool opened = ImGui::TreeNodeEx(node->genericId.c_str(), flags, "%s", displayLabel.c_str());
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();

        // Draw colored Lucide icon & subtle active directory bar
        {
            const FileVisual vis = GetFileVisual(isDirectory, opened, node->assetType, fullPath);
            const float rowH = (std::max)(1.0f, itemMax.y - itemMin.y);
            const float iconX = rowCursorX + ImGui::GetTreeNodeToLabelSpacing();
            ImDrawList *dl = ImGui::GetWindowDrawList();
            if (isCurrentDir || m_SelectedFileTree == fullPath)
            {
                dl->AddRectFilled(ImVec2(itemMin.x, itemMin.y + 2.0f), ImVec2(itemMin.x + 2.0f, itemMax.y - 2.0f), UI::ToU32(UI::GetTheme().primary));
            }
            {
                UI::ScopedFont smallFont(UI::FontVariant::Small);
                const ImVec2 icSz = ImGui::CalcTextSize(vis.icon);
                const float iconY = itemMin.y + glm::round((rowH - icSz.y) * 0.5f);
                dl->AddText(ImVec2(iconX, iconY), UI::ToU32(vis.color), vis.icon);
            }
        }

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

        const auto &theme = UI::GetTheme();
        const float scale = std::max(UI::GetDpiScale(), 1.0f);

        if (m_PendingFocusWindow)
        {
            ImGui::SetNextWindowFocus();
            m_PendingFocusWindow = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * scale, 6.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * scale, 6.0f * scale));
        const bool windowOpen = ImGui::Begin(m_WindowTitle.c_str(), &m_IsOpen);
        m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        if (windowOpen)
        {
            if (!m_EditorLayer->GetActiveProject())
            {
                ImGui::End();
                ImGui::PopStyleVar(2);
                return;
            }

            // Searching variables
            static bool recursiveSearch = true;
            static char buffer[256] = { 0 };
            if (m_ClearSearchRequested)
            {
                buffer[0] = '\0';
                m_CachedSearchQuery.clear();
                m_ClearSearchRequested = false;
            }

            const auto &projectDir = m_EditorLayer->GetActiveProject()->GetDirectory();

            // Rebuild cached navigation metadata only when directory, project root, or tree changes
            if (m_CachedNavTreeVersion != m_TreeCacheVersion
                || m_CachedNavDir != m_CurrentDirectory
                || m_CachedNavProjectDir != projectDir)
            {
                std::error_code eqEc;
                m_CachedIsAtProjectRoot = (m_CurrentDirectory == projectDir)
                    || std::filesystem::equivalent(m_CurrentDirectory, projectDir, eqEc);

                m_CachedCrumbs.clear();
                const std::string rootName = projectDir.filename().string().empty()
                    ? projectDir.generic_string()
                    : projectDir.filename().string();
                m_CachedCrumbs.emplace_back(rootName, projectDir);

                std::error_code relEc;
                const std::filesystem::path relCwd = std::filesystem::relative(m_CurrentDirectory, projectDir, relEc);
                uint32_t currentNodeIdx = 0;
                if (!relEc && !relCwd.empty() && relCwd != "." && !m_TreeNodes.empty())
                {
                    std::filesystem::path accum = projectDir;
                    for (const auto &seg : relCwd)
                    {
                        const std::string segStr = seg.string();
                        if (segStr == "." || segStr.empty())
                            continue;
                        accum /= seg;
                        m_CachedCrumbs.emplace_back(segStr, accum);

                        auto childIt = m_TreeNodes[currentNodeIdx].children.find(seg);
                        if (childIt != m_TreeNodes[currentNodeIdx].children.end())
                            currentNodeIdx = childIt->second;
                    }
                }

                m_CachedCurrentNodeIndex = currentNodeIdx;
                m_CachedNavDir = m_CurrentDirectory;
                m_CachedNavProjectDir = projectDir;
                m_CachedNavTreeVersion = m_TreeCacheVersion;
            }

            // Navigation bar (38px crisp shadcn bar with Lucide icons, breadcrumbs, search & recursive toggle)
            const float navBarH = glm::round(36.0f * scale);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::ToImVec4(theme.card));
            ImGui::PushStyleColor(ImGuiCol_Border, UI::ToImVec4(theme.border));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * scale, 5.0f * scale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f * scale, 0.0f));
            if (ImGui::BeginChild("##NAV_BUTTON_BAR", ImVec2(0.0f, navBarH), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                if (UI::IconButton("##bw_arrow", ICON_LC_ARROW_LEFT, UI::ButtonVariant::Ghost, UI::ButtonSize::IconSm, "Back", false, m_BackwardPathStack.empty()))
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

                ImGui::SameLine(0.0f, 3.0f * scale);
                if (UI::IconButton("##fw_arrow", ICON_LC_ARROW_RIGHT, UI::ButtonVariant::Ghost, UI::ButtonSize::IconSm, "Forward", false, m_ForwardPathStack.empty()))
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

                const bool canGoUp = !m_CachedIsAtProjectRoot && m_CurrentDirectory.has_parent_path();

                ImGui::SameLine(0.0f, 3.0f * scale);
                if (UI::IconButton("##up_dir", ICON_LC_ARROW_UP, UI::ButtonVariant::Ghost, UI::ButtonSize::IconSm, "Parent Folder", false, !canGoUp))
                {
                    const std::filesystem::path parentPath = m_CurrentDirectory.parent_path();
                    if (!parentPath.empty() && parentPath != m_CurrentDirectory)
                    {
                        m_BackwardPathStack.push(m_CurrentDirectory);
                        while (!m_ForwardPathStack.empty())
                        {
                            m_ForwardPathStack.pop();
                        }
                        m_CurrentDirectory = parentPath;
                        m_SelectedFileTree = parentPath;
                    }
                }

                ImGui::SameLine(0.0f, 3.0f * scale);
                if (UI::IconButton("##refresh_bt", ICON_LC_REFRESH_CW, UI::ButtonVariant::Ghost, UI::ButtonSize::IconSm, "Refresh Assets"))
                {
                    m_EditorLayer->GetActiveProject()->ValidateAssetRegistry();
                    PruneMissingNodes(0, projectDir);
                    RefreshAssetTree();
                    CompactTree();
                }

                // Vertical divider
                ImGui::SameLine(0.0f, 6.0f * scale);
                {
                    const ImVec2 sp = ImGui::GetCursorScreenPos();
                    const float lineH = glm::round(18.0f * scale);
                    const float lineY = sp.y + glm::round((glm::round(24.0f * scale) - lineH) * 0.5f);
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(sp.x, lineY), ImVec2(sp.x, lineY + lineH), UI::ToU32(theme.border), 1.0f);
                    ImGui::Dummy(ImVec2(1.0f, glm::round(24.0f * scale)));
                }

                // Breadcrumbs on the left, Search + Recursive on the right
                const float searchW = glm::round(210.0f * scale);
                const float recursiveW = glm::round(96.0f * scale);
                const float rightGroupW = searchW + recursiveW + glm::round(14.0f * scale);

                const float btnH = glm::round(24.0f * scale);
                for (size_t cIdx = 0; cIdx < m_CachedCrumbs.size(); ++cIdx)
                {
                    const float availBeforeRight = ImGui::GetContentRegionAvail().x - rightGroupW;
                    if (cIdx > 0 && availBeforeRight < glm::round(56.0f * scale))
                        break;

                    if (cIdx > 0)
                    {
                        ImGui::SameLine(0.0f, 2.0f * scale);
                        {
                            UI::ScopedFont tinyFont(UI::FontVariant::Tiny);
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextColored(UI::ToImVec4(theme.disabledForeground), "%s", ICON_LC_CHEVRON_RIGHT);
                        }
                    }

                    ImGui::SameLine(0.0f, 2.0f * scale);
                    ImGui::PushID(static_cast<int>(cIdx));
                    const bool isLast = (cIdx + 1 == m_CachedCrumbs.size());
                    const auto variant = isLast ? UI::ButtonVariant::Secondary : UI::ButtonVariant::Ghost;
                    if (UI::Button(m_CachedCrumbs[cIdx].first.c_str(), variant, UI::ButtonSize::Small, ImVec2(0.0f, btnH), cIdx == 0 ? ICON_LC_FOLDER : nullptr))
                    {
                        if (m_CachedCrumbs[cIdx].second != m_CurrentDirectory)
                        {
                            m_BackwardPathStack.push(m_CurrentDirectory);
                            while (!m_ForwardPathStack.empty())
                            {
                                m_ForwardPathStack.pop();
                            }
                            m_CurrentDirectory = m_CachedCrumbs[cIdx].second;
                            m_SelectedFileTree = m_CachedCrumbs[cIdx].second;
                        }
                    }
                    ImGui::PopID();
                }

                const float rightStartX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - rightGroupW - glm::round(8.0f * scale);
                if (rightStartX > ImGui::GetItemRectMax().x + glm::round(8.0f * scale))
                {
                    ImGui::SameLine();
                    ImGui::SetCursorScreenPos(ImVec2(rightStartX, ImGui::GetWindowPos().y + glm::round(5.0f * scale)));
                }
                else
                {
                    ImGui::SameLine(0.0f, 8.0f * scale);
                }

                UI::TextField(
                    "##content_search",
                    buffer,
                    sizeof(buffer),
                    "Search assets...",
                    ImVec2(searchW, btnH),
                    ICON_LC_SEARCH,
                    true,
                    ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_NoHorizontalScroll);

                ImGui::SameLine(0.0f, 6.0f * scale);
                UI::ToggleButton("Recursive", &recursiveSearch, UI::ButtonSize::Small, ImVec2(recursiveW, btnH), ICON_LC_FOLDER_SEARCH);
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            // File tree
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::ToImVec4(theme.card));
                ImGui::PushStyleColor(ImGuiCol_Border, UI::ToImVec4(theme.border));
                ImGui::PushStyleColor(ImGuiCol_Header, UI::ToImVec4(theme.secondaryHover));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::ToImVec4(theme.secondary));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::ToImVec4(theme.secondaryActive));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * scale, 6.0f * scale));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * scale, 1.0f * scale));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * scale, 3.0f * scale));

                if (ImGui::BeginChild("left_item_browser", { glm::round(240.0f * scale), 0.0f }, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
                {
                    if (!m_TreeNodes.empty())
                    {
                        const auto &assetDirectory = projectDir;
                        const std::string rootLabel =
                            assetDirectory.filename().string().empty()
                            ? assetDirectory.generic_string()
                            : assetDirectory.filename().string();

                        const ImGuiTreeNodeFlags rootFlags = (m_SelectedFileTree == assetDirectory ? ImGuiTreeNodeFlags_Selected : 0)
                            | ImGuiTreeNodeFlags_OpenOnArrow
                            | ImGuiTreeNodeFlags_SpanAvailWidth
                            | ImGuiTreeNodeFlags_SpanFullWidth
                            | ImGuiTreeNodeFlags_DefaultOpen;

                        const std::string rootDisplay = "     " + rootLabel;
                        const bool rootOpened = ImGui::TreeNodeEx("##content_browser_root", rootFlags, "%s", rootDisplay.c_str());
                        const ImVec2 rootMin = ImGui::GetItemRectMin();
                        const ImVec2 rootMax = ImGui::GetItemRectMax();
                        {
                            const float rowH = (std::max)(1.0f, rootMax.y - rootMin.y);
                            const float iconX = rootMin.x + ImGui::GetTreeNodeToLabelSpacing();
                            const char *rootIcon = rootOpened ? ICON_LC_FOLDER_OPEN : ICON_LC_FOLDER;
                            ImDrawList *dl = ImGui::GetWindowDrawList();
                            if (m_CurrentDirectory == assetDirectory || m_SelectedFileTree == assetDirectory)
                            {
                                dl->AddRectFilled(ImVec2(rootMin.x, rootMin.y + 2.0f), ImVec2(rootMin.x + 2.0f, rootMax.y - 2.0f), UI::ToU32(theme.primary));
                            }
                            {
                                UI::ScopedFont smallFont(UI::FontVariant::Small);
                                const ImVec2 icSz = ImGui::CalcTextSize(rootIcon);
                                dl->AddText(ImVec2(iconX, rootMin.y + glm::round((rowH - icSz.y) * 0.5f)), UI::ToU32(UI::Accent::Folder), rootIcon);
                            }
                        }

                        DragDropSource(assetDirectory);

                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
                        {
                            UpdateSelection(assetDirectory);
                            m_SelectedFileTree = assetDirectory;
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
                                        QueueMoveCopyPopup(project->GetProjectFilepath(droppedMetadata.filepath), assetDirectory);
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
                                        QueueMoveCopyPopup(std::filesystem::path(dropData->path), assetDirectory);
                                    }
                                }
                            }

                            ImGui::EndDragDropTarget();
                        }

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            if (assetDirectory != m_CurrentDirectory)
                            {
                                m_BackwardPathStack.push(m_CurrentDirectory);
                                while (!m_ForwardPathStack.empty())
                                {
                                    m_ForwardPathStack.pop();
                                }

                                m_CurrentDirectory = assetDirectory;
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
                }
                ImGui::EndChild();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(5);
            }

            ImGui::SameLine(0.0f, 4.0f * scale);

            // File list (asset grid cards)
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, UI::ToImVec4(theme.background));
                ImGui::PushStyleColor(ImGuiCol_Border, UI::ToImVec4(theme.border));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * scale, 10.0f * scale));
                if (ImGui::BeginChild("##file_lists", { 0.0f, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding))
                {
                    FileTreeNode *node = (!m_TreeNodes.empty() && m_CachedCurrentNodeIndex < m_TreeNodes.size())
                        ? &m_TreeNodes[m_CachedCurrentNodeIndex]
                        : m_TreeNodes.data();
                    if (node)
                    {
                        const std::string_view searchView(buffer);
                        const bool searchActive = !searchView.empty();
                        if (!searchActive && node->children.empty())
                            UI::DrawCenteredText("Empty folder", UI::ToU32(theme.mutedForeground));

                        if (searchActive)
                        {
                            if (m_CachedSearchTreeVersion != m_TreeCacheVersion
                                || m_CachedSearchDir != m_CurrentDirectory
                                || m_CachedSearchRecursive != recursiveSearch
                                || m_CachedSearchQuery != searchView)
                            {
                                m_CachedSearchQuery.assign(searchView);
                                m_CachedSearchRecursive = recursiveSearch;
                                m_CachedSearchDir = m_CurrentDirectory;
                                m_CachedSearchTreeVersion = m_TreeCacheVersion;
                                m_CachedSearchNodeIndices.clear();
                                m_CachedSearchRelPaths.clear();

                                const std::string searchLower = stringutils::ToLower(m_CachedSearchQuery);
                                if (recursiveSearch)
                                {
                                    for (const auto &treeNode : m_TreeNodes)
                                    {
                                        for (const uint32_t childNodeIndex : treeNode.sortedChildren)
                                        {
                                            const FileTreeNode &child = m_TreeNodes[childNodeIndex];
                                            std::error_code relEc;
                                            const std::filesystem::path rel = std::filesystem::relative(child.fullPath, m_CurrentDirectory, relEc);
                                            const std::string pathLower = stringutils::ToLower(rel.generic_string());
                                            if (pathLower.find(searchLower) != std::string::npos)
                                            {
                                                m_CachedSearchNodeIndices.push_back(childNodeIndex);
                                                m_CachedSearchRelPaths.push_back(rel);
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    for (const uint32_t childNodeIndex : node->sortedChildren)
                                    {
                                        const FileTreeNode &child = m_TreeNodes[childNodeIndex];
                                        const std::string pathLower = stringutils::ToLower(child.path.generic_string());
                                        if (pathLower.find(searchLower) != std::string::npos)
                                        {
                                            m_CachedSearchNodeIndices.push_back(childNodeIndex);
                                            m_CachedSearchRelPaths.push_back(child.path);
                                        }
                                    }
                                }
                            }
                        }

                        const float spacing = glm::round(10.0f * scale);
                        const float cardPad = glm::round(6.0f * scale);
                        const float thumbSize = static_cast<float>(m_ThumbnailSize);
                        const float itemWidth = thumbSize + cardPad * 2.0f;
                        const float labelH = glm::round(32.0f * scale);
                        const float cardHeight = cardPad + thumbSize + glm::round(6.0f * scale) + labelH + glm::round(4.0f * scale);
                        const float contentWidth = ImGui::GetContentRegionAvail().x;

                        const int itemPerRow = std::max(1, static_cast<int>((contentWidth + spacing) / (itemWidth + spacing)));
                        const auto itemCount = searchActive
                            ? static_cast<int>(m_CachedSearchNodeIndices.size())
                            : static_cast<int>(node->sortedChildren.size());

                        if (searchActive && itemCount == 0)
                            UI::DrawCenteredText("No matching items", UI::ToU32(theme.mutedForeground));

                        const int rowCount = (itemCount + itemPerRow - 1) / itemPerRow;
                        const float rowHeight = cardHeight + spacing;
                        const float cursorStartY = ImGui::GetCursorPosY();

                        if (!m_PendingScrollToPath.empty() && !searchActive)
                        {
                            int foundIdx = -1;
                            for (int idx = 0; idx < itemCount; ++idx)
                            {
                                const std::filesystem::path nodePath = m_TreeNodes[node->sortedChildren[static_cast<size_t>(idx)]].fullPath.lexically_normal();
                                std::error_code eqEc;
                                if (nodePath == m_PendingScrollToPath || std::filesystem::equivalent(nodePath, m_PendingScrollToPath, eqEc))
                                {
                                    foundIdx = idx;
                                    break;
                                }
                            }
                            if (foundIdx >= 0)
                            {
                                const int targetRow = foundIdx / itemPerRow;
                                const float targetScrollY = std::max(0.0f, static_cast<float>(targetRow) * rowHeight - ImGui::GetWindowHeight() * 0.35f);
                                ImGui::SetScrollY(targetScrollY);
                            }
                            m_PendingScrollToPath.clear();
                        }

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
                                    if (i > rowStart)
                                        ImGui::SameLine(0.0f, spacing);

                                    const size_t idx = static_cast<size_t>(i);
                                    const FileTreeNode *cachedNode = searchActive
                                        ? &m_TreeNodes[m_CachedSearchNodeIndices[idx]]
                                        : &m_TreeNodes[node->sortedChildren[idx]];
                                    const std::filesystem::path &item = searchActive
                                        ? m_CachedSearchRelPaths[idx]
                                        : cachedNode->path;

                                    ImGui::PushID(cachedNode->genericId.c_str());
                                    UIRenderFileButton(item, cachedNode);
                                    ImGui::PopID();
                                }
                            }
                        }

                        ImGui::SetCursorPosY(cursorStartY);
                        const ImVec2 availDrop = ImGui::GetContentRegionAvail();
                        const float minGridHeight = static_cast<float>(rowCount) * rowHeight;
                        ImGui::Dummy(ImVec2(std::max(availDrop.x, 50.0f), std::max({ availDrop.y, minGridHeight, 50.0f })));

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
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
            }

            // ==== Popup modals ====
            UIRenderPopupModals();
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
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
                const AssetType assetType = GetAssetTypeFromExtension(filepath.extension().string());
                if (ThumbnailGenerator::IsSupportedAssetType(assetType))
                {
                    StartSceneThumbnailLoad(filepath, assetType);
                }
                else
                {
                    StartThumbnailLoad(filepath);
                }
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
        if (UI::BeginWindowContextMenu("##content_browser_context_menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoReopen | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (UI::ContextMenuBeginSubMenu("Create", ICON_LC_CIRCLE_PLUS, true, UI::Accent::Mesh))
            {
                if (UI::ContextMenuItem("New Folder", nullptr, ICON_LC_FOLDER_PLUS, false, true, UI::Accent::Folder))
                {
                    m_ShowCreateFolderModal = true;
                }

                UI::ContextMenuSeparator();

                if (UI::ContextMenuItem("C# Script", nullptr, ICON_LC_FILE_CODE_2, false, true, UI::Accent::ScriptCS))
                {
                    m_ShowCreateScriptModal = true;
                }

                if (UI::ContextMenuItem("Lua Script", nullptr, ICON_LC_FILE_CODE, false, true, UI::Accent::ScriptLua))
                {
                    m_ShowCreateLuaScriptModal = true;
                }

                if (UI::ContextMenuItem("Prefab", nullptr, ICON_LC_PACKAGE, false, true, UI::Accent::Mesh))
                {
                    CreateNewPrefab();
                }

                if (UI::ContextMenuItem("Sprite Sheet", nullptr, ICON_LC_GRID_2X2, false, true, UI::Accent::Material))
                {
                    DispatchCreateAssetEditorEvent(AssetType::SpriteSheet, m_CurrentDirectory);
                }

                if (UI::ContextMenuItem("Widget", nullptr, ICON_LC_LAYOUT_TEMPLATE, false, true, UI::Accent::Widget))
                {
                    DispatchCreateAssetEditorEvent(AssetType::Widget, m_CurrentDirectory);
                }

                if (UI::ContextMenuBeginSubMenu("Animation", ICON_LC_FILM, true, UI::Accent::Animation))
                {
                    if (UI::ContextMenuItem("Blend Space 2D", nullptr, ICON_LC_SLIDERS_HORIZONTAL, false, true, UI::Accent::Animation))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::BlendSpace, m_CurrentDirectory);
                    }

                    if (UI::ContextMenuItem("Animator Controller", nullptr, ICON_LC_CLAPPERBOARD, false, true, UI::Accent::Animation))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::AnimatorController, m_CurrentDirectory);
                    }

                    if (UI::ContextMenuItem("Animation 2D", nullptr, ICON_LC_FILM, false, true, UI::Accent::Animation))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::Animation2D, m_CurrentDirectory);
                    }

                    if (UI::ContextMenuItem("Animator Controller 2D", nullptr, ICON_LC_CLAPPERBOARD, false, true, UI::Accent::Animation))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::AnimatorController2D, m_CurrentDirectory);
                    }

                    UI::ContextMenuEndSubMenu();
                }

                if (UI::ContextMenuBeginSubMenu("Material", ICON_LC_PALETTE, true, UI::Accent::Material))
                {
                    if (UI::ContextMenuItem("Material2D", nullptr, ICON_LC_PAINTBRUSH, false, true, UI::Accent::Material))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::Material2D, m_CurrentDirectory);
                    }

                    if (UI::ContextMenuItem("Material", nullptr, ICON_LC_PALETTE, false, true, UI::Accent::Material))
                    {
                        DispatchCreateAssetEditorEvent(AssetType::Material, m_CurrentDirectory);
                    }

                    UI::ContextMenuEndSubMenu();
                }

                if (UI::ContextMenuItem("Terrain", nullptr, ICON_LC_MOUNTAIN, false, true, UI::Accent::Environment))
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
                        UI::ContextMenuSeparator();
                        if (UI::ContextMenuBeginSubMenu("Scriptable Object", ICON_LC_BRACES, true, UI::Accent::ScriptCS))
                        {
                            for (const auto &entry : *menuEntries)
                            {
                                const std::string &label = entry.menuName.empty() ? entry.className : entry.menuName;
                                if (UI::ContextMenuItem(label.c_str(), nullptr, ICON_LC_BRACES, false, true, UI::Accent::ScriptCS))
                                {
                                    m_PendingScriptableObjectClassName = entry.className;
                                    m_PendingScriptableObjectFileName = entry.fileName;
                                    std::strncpy(m_PopupInputBuffer, entry.fileName.c_str(), sizeof(m_PopupInputBuffer) - 1);
                                    m_ShowCreateScriptableObjectModal = true;
                                }
                            }
                            UI::ContextMenuEndSubMenu();
                        }
                    }
                }

                UI::ContextMenuEndSubMenu();
            }

            if (UI::ContextMenuBeginSubMenu("Import", ICON_LC_IMPORT, true, UI::Accent::Camera))
            {
                if (UI::ContextMenuItem("Import to this directory", nullptr, ICON_LC_DOWNLOAD, false, true, UI::Accent::Camera))
                {
                    SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                        Application::GetInstance()->GetWindow()->GetWindowHandle(),
                        kExtFilters, IM_ARRAYSIZE(kExtFilters),
                        nullptr, true
                    );
                }
                UI::ContextMenuEndSubMenu();
            }

            if (UI::ContextMenuBeginSubMenu("Thumbnail Size", ICON_LC_LAYOUT_GRID, true, UI::Accent::Widget))
            {
                if (UI::ContextMenuItem("Small (64px)", nullptr, m_ThumbnailSize == 64 ? ICON_LC_CHECK : " ")) m_ThumbnailSize = 64;
                if (UI::ContextMenuItem("Medium (72px)", nullptr, m_ThumbnailSize == 72 ? ICON_LC_CHECK : " ")) m_ThumbnailSize = 72;
                if (UI::ContextMenuItem("Large (96px)", nullptr, m_ThumbnailSize == 96 ? ICON_LC_CHECK : " ")) m_ThumbnailSize = 96;
                if (UI::ContextMenuItem("Huge (120px)", nullptr, m_ThumbnailSize == 120 ? ICON_LC_CHECK : " ")) m_ThumbnailSize = 120;
                UI::ContextMenuEndSubMenu();
            }

            UI::ContextMenuSeparator();

            if (UI::ContextMenuItem("Refresh", nullptr, ICON_LC_REFRESH_CW, false, true, UI::Accent::Environment))
            {
                m_EditorLayer->GetActiveProject()->ValidateAssetRegistry();
                PruneMissingNodes(0, m_EditorLayer->GetActiveProject()->GetDirectory());
                RefreshAssetTree();
                CompactTree();
            }

#ifdef PLATFORM_WINDOWS
            if (UI::ContextMenuItem("Open in File Explorer", nullptr, ICON_LC_EXTERNAL_LINK, false, true, UI::Accent::Folder))
            {
                std::string command = std::format("explorer {}", m_CurrentDirectory.string());
                std::system(command.c_str());
            }
#endif
            UI::EndContextMenu();
        }
    }

    void ContentBrowserPanel::UIRenderPopupModals()
    {
        const auto &theme = UI::GetTheme();
        const float scale = std::max(UI::GetDpiScale(), 1.0f);

        ImGui::PushStyleColor(ImGuiCol_PopupBg, UI::ToImVec4(theme.popover));
        ImGui::PushStyleColor(ImGuiCol_Border, UI::ToImVec4(theme.border));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * scale, 14.0f * scale));

        // Create Folder Modal
        if (m_ShowCreateFolderModal)
        {
            ImGui::OpenPopup("Create Folder");
            m_ShowCreateFolderModal = false;
        }

        static bool focusCreateFolder = true;
        if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            {
                UI::ScopedFont smallFont(UI::FontVariant::Small);
                ImGui::TextColored(UI::ToImVec4(theme.mutedForeground), "Location: %s", m_CurrentDirectory.generic_string().c_str());
            }
            ImGui::Spacing();
            if (focusCreateFolder)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateFolder = false;
            }
            const bool submitByEnter = UI::TextField("##folder_name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), "Folder name...", ImVec2(320.0f * scale, 28.0f * scale), ICON_LC_FOLDER_PLUS, true, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (submitByEnter || UI::Button("Create", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0, 0), ICON_LC_CHECK))
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
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button("Cancel", UI::ButtonVariant::Outline, UI::ButtonSize::Small))
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
            {
                UI::ScopedFont smallFont(UI::FontVariant::Small);
                ImGui::TextColored(UI::ToImVec4(theme.mutedForeground), "Location: %s", m_CurrentDirectory.generic_string().c_str());
            }
            ImGui::Spacing();
            if (focusCreateScript)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateScript = false;
            }
            const bool submitByEnter = UI::TextField("##cs_name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), "ScriptName.cs", ImVec2(320.0f * scale, 28.0f * scale), ICON_LC_FILE_CODE_2, true, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (submitByEnter || UI::Button("Create", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0, 0), ICON_LC_CHECK))
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
            if (UI::Button("Cancel", UI::ButtonVariant::Outline, UI::ButtonSize::Small))
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

        // Create Lua Script Modal
        if (m_ShowCreateLuaScriptModal)
        {
            ImGui::OpenPopup("Create Lua Script");
            m_ShowCreateLuaScriptModal = false;
        }

        static bool focusCreateLuaScript = true;
        if (ImGui::BeginPopupModal("Create Lua Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            {
                UI::ScopedFont smallFont(UI::FontVariant::Small);
                ImGui::TextColored(UI::ToImVec4(theme.mutedForeground), "Location: %s", m_CurrentDirectory.generic_string().c_str());
            }
            ImGui::Spacing();
            if (focusCreateLuaScript)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateLuaScript = false;
            }
            const bool submitByEnter = UI::TextField("##lua_name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), "script_name.lua", ImVec2(320.0f * scale, 28.0f * scale), ICON_LC_FILE_CODE, true, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (submitByEnter || UI::Button("Create", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0, 0), ICON_LC_CHECK))
            {
                std::string name = m_PopupInputBuffer;
                if (!name.empty())
                {
                    if (!name.ends_with(".lua")) name += ".lua";
                    std::filesystem::path newPath = m_CurrentDirectory / name;
                    if (!std::filesystem::exists(newPath))
                    {
                        m_EditorLayer->GetActiveProject()->CreateLuaScript(newPath);
                        Application::SubmitToMainThread([this]() { RefreshFiles(); });
                    }
                }
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button("Cancel", UI::ButtonVariant::Outline, UI::ButtonSize::Small))
            {
                m_PopupInputBuffer[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            focusCreateLuaScript = true;
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
            {
                UI::ScopedFont smallFont(UI::FontVariant::Small);
                ImGui::TextColored(UI::ToImVec4(theme.mutedForeground), "Location: %s", m_CurrentDirectory.generic_string().c_str());
            }
            ImGui::Spacing();
            if (focusCreateSO)
            {
                ImGui::SetKeyboardFocusHere();
                focusCreateSO = false;
            }
            const bool submitByEnter = UI::TextField("##so_name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), "Asset name...", ImVec2(320.0f * scale, 28.0f * scale), ICON_LC_BRACES, true, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            if (submitByEnter || UI::Button("Create", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0, 0), ICON_LC_CHECK))
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
            if (UI::Button("Cancel", UI::ButtonVariant::Outline, UI::ButtonSize::Small))
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
                auto &itemPath = m_SelectedItems.front();

                {
                    UI::ScopedFont smallFont(UI::FontVariant::Small);
                    ImGui::TextColored(UI::ToImVec4(theme.mutedForeground), "Rename: %s", itemPath.filename().generic_string().c_str());
                }
                ImGui::Spacing();
                if (focusRename)
                {
                    ImGui::SetKeyboardFocusHere();
                    focusRename = false;
                }
                const bool submitByEnter = UI::TextField("##rename_name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer), "New name...", ImVec2(320.0f * scale, 28.0f * scale), ICON_LC_PENCIL, true, ImGuiInputTextFlags_EnterReturnsTrue);

                ImGui::Spacing();
                const bool submitByButton = UI::Button("Rename", UI::ButtonVariant::Primary, UI::ButtonSize::Small, ImVec2(0, 0), ICON_LC_CHECK);
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
                if (UI::Button("Cancel", UI::ButtonVariant::Outline, UI::ButtonSize::Small))
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

            ImGui::Text("Are you sure you want to delete %zu item(s)?", itemCount);
            ImGui::Spacing();

            for (const auto &item : m_SelectedItems)
            {
                UI::ScopedFont monoFont(UI::FontVariant::Mono);
                ImGui::TextColored(UI::ToImVec4(theme.mutedForeground), "  %s", item.filename().generic_string().c_str());
            }

            ImGui::Spacing();
            if (UI::Button("Delete", UI::ButtonVariant::Destructive, UI::ButtonSize::Small, ImVec2(0, 0), ICON_LC_TRASH_2))
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
                            std::filesystem::remove(itemPath.string(), ec);

                            const std::filesystem::path metaPath = itemPath.string() + ".meta";
                            if (std::filesystem::exists(metaPath))
                            {
                                std::error_code metaEc;
                                std::filesystem::remove(metaPath.string(), metaEc);

                                const std::filesystem::path metaRelative = project->GetProjectFilepath(metaPath);
                                const AssetHandle metaHandle = m_AssetManager->GetAssetHandle(metaRelative);
                                if (metaHandle != AssetHandle(0))
                                {
                                    filesToRemove.emplace_back(FileToRemove{ metaHandle, metaPath });
                                }
                            }

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

                    Application::SubmitToMainThread([this, genCSProjectRequested]()
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
            if (UI::Button("Cancel", UI::ButtonVariant::Outline, UI::ButtonSize::Small))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        if (m_ShowMoveCopyPopup)
        {
            UI::OpenPopupMenu("Move/Copy Item");
            m_ShowMoveCopyPopup = false;
        }

        if (UI::BeginPopupMenu("Move/Copy Item"))
        {
            const size_t sourceCount = m_PendingDragDropSources.size();
            const std::string srcDesc = (sourceCount == 1)
                ? m_PendingDragDropSources.front().filename().generic_string()
                : std::format("{} items", sourceCount);
            const std::string headerDesc = std::format("{} -> {}", srcDesc, m_PendingDragDropTargetDirectory.filename().generic_string());
            UI::ContextMenuLabel(headerDesc.c_str());
            UI::ContextMenuSeparator();

            if (UI::ContextMenuItem("Move Here", nullptr, ICON_LC_FOLDER_INPUT, false, true, UI::Accent::Folder))
            {
                AssetWorker::SubmitJob([this]
                {
                    MoveOrCopySelectionToDirectory(m_PendingDragDropTargetDirectory, true);
                    m_PendingDragDropSources.clear();
                    m_PendingDragDropTargetDirectory.clear();
                });
            }

            if (UI::ContextMenuItem("Copy Here", nullptr, ICON_LC_COPY, false, true, UI::Accent::Camera))
            {
                AssetWorker::SubmitJob([this]
                {
                    MoveOrCopySelectionToDirectory(m_PendingDragDropTargetDirectory, false);
                    m_PendingDragDropSources.clear();
                    m_PendingDragDropTargetDirectory.clear();
                });
            }

            UI::EndContextMenu();
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
            FileTreeNode &node = m_TreeNodes[i];
            if (!node.isDeleted && !node.path.empty())
            {
                node.relativePath = GetNodeFullpath(i);
                node.fullPath = assetDir / node.relativePath;
                node.filename = node.path.filename().string();
                node.genericId = node.fullPath.generic_string();
                std::error_code ec;
                node.isDirectory = std::filesystem::is_directory(node.fullPath, ec);
                node.assetType = node.isDirectory ? AssetType::Invalid : GetAssetTypeFromExtension(node.path.extension().string());
            }
            else
            {
                node.relativePath.clear();
                node.fullPath.clear();
                node.filename.clear();
                node.genericId.clear();
                node.isDirectory = false;
                node.assetType = AssetType::Invalid;
            }
        }

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

            std::ranges::sort(m_TreeNodes[i].sortedChildren, [this](uint32_t leftIndex, uint32_t rightIndex)
            {
                const FileTreeNode &leftNode = m_TreeNodes[leftIndex];
                const FileTreeNode &rightNode = m_TreeNodes[rightIndex];
                if (leftNode.isDirectory != rightNode.isDirectory)
                {
                    return leftNode.isDirectory;
                }

                return leftNode.path.generic_string() < rightNode.path.generic_string();
            });
        }

        for (uint32_t i = 1; i < m_TreeNodes.size(); ++i)
        {
            if (m_TreeNodes[i].parent == 0 && !m_TreeNodes[i].isDeleted)
            {
                m_SortedRootNodeIndices.push_back(i);
            }
        }

        std::ranges::sort(m_SortedRootNodeIndices, [this](uint32_t leftIndex, uint32_t rightIndex)
        {
            const FileTreeNode &leftNode = m_TreeNodes[leftIndex];
            const FileTreeNode &rightNode = m_TreeNodes[rightIndex];
            if (leftNode.isDirectory != rightNode.isDirectory)
            {
                return leftNode.isDirectory;
            }

            return leftNode.path.generic_string() < rightNode.path.generic_string();
        });

        ++m_TreeCacheVersion;
        m_CachedNavTreeVersion = 0;
        m_CachedSearchTreeVersion = 0;
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

    void ContentBrowserPanel::UIRenderFileButton(const std::filesystem::path &item, const FileTreeNode *cachedNode)
    {
        IGN_PROFILE_SCOPE_COLOR("ContentBrowser::UIRenderFileButton", 0xCD5C5C);

        std::filesystem::path fallbackPath;
        const std::filesystem::path &path = cachedNode ? cachedNode->fullPath : (fallbackPath = m_CurrentDirectory / item);
        bool isDirectory = false;
        AssetType itemAssetType = AssetType::Invalid;
        if (cachedNode)
        {
            isDirectory = cachedNode->isDirectory;
            itemAssetType = cachedNode->assetType;
        }
        else
        {
            std::error_code statusError;
            const std::filesystem::file_status fileStatus = std::filesystem::status(path.string(), statusError);
            if (statusError || !std::filesystem::exists(fileStatus))
                return;
            isDirectory = std::filesystem::is_directory(fileStatus);
            itemAssetType = GetAssetTypeFromExtension(item.extension().string());
        }
        const FileVisual visual = GetFileVisual(isDirectory, false, itemAssetType, path);

        // Request/touch thumbnail cache (actual GPU texture used if available)
        Ref<Texture> icon = GetOrCreateThumbnail(path, isDirectory);
        Ref<Texture> loadedThumbnail = nullptr;
        if (!isDirectory)
        {
            if (auto thumbIt = s_SharedThumbnails.find(path); thumbIt != s_SharedThumbnails.end() && thumbIt->second.thumbnail)
            {
                loadedThumbnail = thumbIt->second.thumbnail;
            }
        }

        const auto &theme = UI::GetTheme();
        const float scale = std::max(UI::GetDpiScale(), 1.0f);
        const float cardPad = glm::round(6.0f * scale);
        const float thumbSize = static_cast<float>(m_ThumbnailSize);
        const float cardW = thumbSize + cardPad * 2.0f;
        const float labelH = glm::round(32.0f * scale);
        const float cardH = cardPad + thumbSize + glm::round(6.0f * scale) + labelH + glm::round(4.0f * scale);

        ImGui::BeginGroup();

        const ImVec2 cardMin = ImGui::GetCursorScreenPos();
        const ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);
        const ImVec2 wellMin(cardMin.x + cardPad, cardMin.y + cardPad);
        const ImVec2 wellMax(wellMin.x + thumbSize, wellMin.y + thumbSize);

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton(item.string().c_str(), ImVec2(cardW, cardH));

        const bool isActive = ImGui::IsItemActive();
        const bool isHovered = ImGui::IsItemHovered();
        const bool isSelected = std::ranges::find(m_SelectedItems, path) != m_SelectedItems.end();

        if (isHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsItemHovered())
        {
            UpdateSelection(path);
            m_SelectedFileTree = path;
        }

        // Card container
        const glm::vec4 cardBg = isSelected ? theme.secondaryHover : (isActive ? theme.secondaryActive : (isHovered ? theme.secondary : theme.card));
        const glm::vec4 cardBorder = isSelected ? theme.primary : (isHovered ? theme.borderStrong : theme.borderSubtle);
        drawList->AddRectFilled(cardMin, cardMax, UI::ToU32(cardBg), 0.0f);
        drawList->AddRect(cardMin, cardMax, UI::ToU32(cardBorder), 0.0f, 0, isSelected ? 1.5f : 1.0f);

        // Thumbnail / Icon well
        drawList->AddRectFilled(wellMin, wellMax, UI::ToU32(theme.inputBg), 0.0f);
        drawList->AddRect(wellMin, wellMax, UI::ToU32(theme.border), 0.0f, 0, 1.0f);

        if (loadedThumbnail && loadedThumbnail->GetShaderResourceView())
        {
            const ImVec2 displaySize = CalculateThumbnailDisplaySize(loadedThumbnail, thumbSize - 4.0f);
            const ImVec2 imageMin(
                wellMin.x + (thumbSize - displaySize.x) * 0.5f,
                wellMin.y + (thumbSize - displaySize.y) * 0.5f);
            const ImVec2 imageMax(imageMin.x + displaySize.x, imageMin.y + displaySize.y);
            drawList->AddImage(UI::GetTextureID(loadedThumbnail), imageMin, imageMax);
        }
        else
        {
            // Clean vector Lucide emblem inside well (for folders, code scripts, audio, or pending thumbnails)
            const float badgeSize = glm::round(glm::clamp(thumbSize * 0.50f, 28.0f, 44.0f));
            const ImVec2 bMin(wellMin.x + glm::round((thumbSize - badgeSize) * 0.5f), wellMin.y + glm::round((thumbSize - badgeSize) * 0.5f));
            const ImVec2 bMax(bMin.x + badgeSize, bMin.y + badgeSize);
            drawList->AddRectFilled(bMin, bMax, UI::ToU32(theme.secondary), 0.0f);
            drawList->AddRect(bMin, bMax, UI::ToU32(UI::WithAlpha(visual.color, 0.45f)), 0.0f, 0, 1.0f);

            {
                UI::ScopedFont largeFont(UI::FontVariant::Bold);
                const ImVec2 icSz = ImGui::CalcTextSize(visual.icon);
                drawList->AddText(
                    ImVec2(bMin.x + glm::round((badgeSize - icSz.x) * 0.5f), bMin.y + glm::round((badgeSize - icSz.y) * 0.5f)),
                    UI::ToU32(visual.color),
                    visual.icon);
            }
        }

        // Subtle 2px semantic bottom accent bar on preview well
        drawList->AddRectFilled(ImVec2(wellMin.x, wellMax.y - 2.0f), wellMax, UI::ToU32(visual.color, isSelected ? 0.95f : 0.70f));

        // Filename (clipped cleanly) & asset type subtitle
        const std::string displayName = isDirectory ? item.filename().generic_string() : item.stem().generic_string();
        const float textX = cardMin.x + cardPad;
        const float textY = wellMax.y + glm::round(5.0f * scale);
        const ImVec4 nameClip(textX, textY, cardMax.x - cardPad, cardMax.y);
        {
            UI::ScopedFont smallFont(UI::FontVariant::Small);
            drawList->AddText(nullptr, 0.0f, ImVec2(textX, textY), UI::ToU32(isSelected ? theme.primary : theme.foreground), displayName.c_str(), nullptr, 0.0f, &nameClip);
        }
        {
            UI::ScopedFont tinyFont(UI::FontVariant::Tiny);
            const float subY = textY + glm::round(15.0f * scale);
            drawList->AddText(nullptr, 0.0f, ImVec2(textX, subY), UI::ToU32(theme.mutedForeground), visual.typeBadge, nullptr, 0.0f, &nameClip);
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
        if (UI::BeginContextMenu())
        {
            UI::ContextMenuLabel(item.filename().generic_string().c_str());
            UI::ContextMenuSeparator();

            if (UI::ContextMenuItem("Open", nullptr, isDirectory ? ICON_LC_FOLDER_OPEN : ICON_LC_EXTERNAL_LINK, false, true, visual.color))
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
                    const bool isScript = metadata.type == AssetType::LuaScript || metadata.filepath.extension() == ".cs";
                    if (isScript || !DispatchOpenAssetEditorEvent(handle, metadata))
                    {
                        std::string command = std::format("\"{}\"", path.generic_string());
                        std::system(command.c_str());
                    }
                }
            }

            if (!isDirectory)
            {
                if (UI::ContextMenuItem("Reimport Asset", nullptr, ICON_LC_DOWNLOAD, false, true, UI::Accent::Camera))
                {
                    m_AssetManager->ImportAsset(path);
                }
            }

            if (UI::ContextMenuItem("Rename", "F2", ICON_LC_PENCIL))
            {
                std::string fname = std::filesystem::is_directory(path) ? path.filename().generic_string() : path.stem().generic_string();
                std::strncpy(m_PopupInputBuffer, fname.c_str(), sizeof(m_PopupInputBuffer) - 1);
                m_ShowRenameModal = true;
            }

            if (!isDirectory && UI::ContextMenuItem("Duplicate", "Ctrl+D", ICON_LC_COPY, false, true, UI::Accent::Camera))
            {
                DuplicateItem(path);
            }

            if (!isDirectory)
            {
                switch (itemAssetType)
                {
                    case AssetType::Scene:
                    {
                        UI::ContextMenuSeparator();
                        if (UI::ContextMenuItem("Set As Default Scene", nullptr, ICON_LC_STAR, false, true, UI::Accent::Folder))
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
                        break;
                    }
                    case AssetType::SkeletalAnimation:
                    {
                        UI::ContextMenuSeparator();
                        if (UI::ContextMenuItem("Create Animation Montage", nullptr, ICON_LC_FILM, false, true, UI::Accent::Animation))
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
                        break;
                    }
                    default: break;
                }
            }

            UI::ContextMenuSeparator();
            if (UI::ContextMenuItem("Delete", "Del", ICON_LC_TRASH_2, true))
            {
                if (std::ranges::find(m_SelectedItems, path) == m_SelectedItems.end())
                {
                    UpdateSelection(path);
                }
                m_ShowDeleteModal = true;
            }

            UI::EndContextMenu();
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

        // Render sprite sheet overlay trigger + popup without shifting grid geometry
        if (!isDirectory && itemAssetType == AssetType::SpriteSheet)
        {
            AssetHandle handle = m_AssetManager->GetAssetHandle(path);
            AssetMetaData metadata = m_AssetManager->GetMetaData(handle);

            if (metadata.type == AssetType::SpriteSheet && handle != AssetHandle(0))
            {
                const std::string popupId = std::format("##sprites_popup_{}", item.generic_string());
                ImGui::SetCursorScreenPos(ImVec2(wellMax.x - glm::round(24.0f * scale), wellMin.y + glm::round(2.0f * scale)));
                if (UI::IconButton("##sprites_btn", ICON_LC_GRID_2X2, UI::ButtonVariant::Secondary, UI::ButtonSize::IconSm, "Browse Sprites"))
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

                                if (texture && texture->GetShaderResourceView())
                                {
                                    ImTextureID texId = UI::GetTextureID(texture);
                                    ImGui::ImageButton("##sprite_item", texId, ImVec2(spritePreviewSize, spritePreviewSize), ImVec2(sprite.uv0.x, sprite.uv0.y), ImVec2(sprite.uv1.x, sprite.uv1.y));
                                }
                                else
                                {
                                    UI::Button("N/A", UI::ButtonVariant::Outline, UI::ButtonSize::Small, ImVec2(spritePreviewSize, spritePreviewSize));
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

            if (dragIcon && dragIcon->GetShaderResourceView())
            {
                ImTextureID iconId = UI::GetTextureID(dragIcon);
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

    Ref<Texture> ContentBrowserPanel::GetFallbackIcon(AssetType type, const std::filesystem::path &filepath) const
    {
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
            case AssetType::StaticMesh: return s_SharedIcons["st_mesh"];
            case AssetType::Animation2D: return s_SharedIcons["anim_2d"];
            case AssetType::AnimatorController: return s_SharedIcons["anim_ctrl"];
            case AssetType::AnimatorController2D: return s_SharedIcons["anim_ctrl_2d"];
            case AssetType::Terrain: return s_SharedIcons["mesh"];
            case AssetType::LuaScript: return s_SharedIcons["lua"];
            default: break;
        }

        if (filepath.extension() == ".cs")
            return s_SharedIcons["csharp"];

        return s_SharedIcons["unknown"];
    }

    Ref<Texture> ContentBrowserPanel::GetOrCreateThumbnail(const std::filesystem::path &filepath, bool isDirectory)
    {
        IGN_PROFILE_FUNCTION();

        if (isDirectory)
            return s_SharedIcons["folder"];

        const AssetType type = GetAssetTypeFromExtension(filepath.extension().string());
        const bool isSceneThumbnailAsset = ThumbnailGenerator::IsSupportedAssetType(type);
        const bool isImageAsset = (!isSceneThumbnailAsset && IsImageFile(filepath));

        if (!isSceneThumbnailAsset && !isImageAsset)
        {
            return GetFallbackIcon(type, filepath);
        }

        auto it = s_SharedThumbnails.find(filepath);
        if (it != s_SharedThumbnails.end())
        {
            // Mark as used this frame
            it->second.lastFrameUsed = s_SharedCurrentFrame;

            if (it->second.thumbnail)
            {
                return it->second.thumbnail;
            }

            if (!s_SharedThumbnailLoadsInFlight.contains(filepath))
            {
                s_SharedPendingThumbnailLoads.push(filepath);
                s_SharedThumbnailLoadsInFlight.insert(filepath);
            }

            return GetFallbackIcon(type, filepath);
        }

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

        return GetFallbackIcon(type, filepath);
    }

    Ref<Texture> ContentBrowserPanel::GetAssetThumbnail(AssetHandle handle, AssetManager *assetManager, Project *project)
    {
        if (handle == AssetHandle(0) || !assetManager || !project)
            return nullptr;

        const AssetMetaData meta = assetManager->GetMetaData(handle);
        if (meta.filepath.empty())
            return nullptr;

        const std::filesystem::path fullPath = project->GetProjectFilepath(meta.filepath).lexically_normal();
        auto it = s_SharedThumbnails.find(fullPath);
        if (it != s_SharedThumbnails.end())
        {
            it->second.lastFrameUsed = s_SharedCurrentFrame;
            if (it->second.thumbnail && it->second.thumbnail->GetShaderResourceView())
                return it->second.thumbnail;
        }

        const bool isSceneThumbnailAsset = ThumbnailGenerator::IsSupportedAssetType(meta.type);
        const bool isImageAsset = (!isSceneThumbnailAsset && meta.type == AssetType::Texture);
        if (!isSceneThumbnailAsset && !isImageAsset)
            return nullptr;

        if (it == s_SharedThumbnails.end())
        {
            FileThumbnail placeholder;
            placeholder.thumbnail = nullptr;
            placeholder.timestamp = 0;
            placeholder.lastFrameUsed = s_SharedCurrentFrame;
            s_SharedThumbnails[fullPath] = placeholder;
        }

        if (!s_SharedThumbnailLoadsInFlight.contains(fullPath))
        {
            s_SharedPendingThumbnailLoads.push(fullPath);
            s_SharedThumbnailLoadsInFlight.insert(fullPath);
        }

        return nullptr;
    }

    void ContentBrowserPanel::StartSceneThumbnailLoad(const std::filesystem::path &filepath, AssetType type)
    {
        IGN_PROFILE_FUNCTION();

        Ref<Project> project = m_EditorLayer ? m_EditorLayer->GetActiveProject() : nullptr;
        if (!project || !m_AssetManager)
        {
            s_SharedThumbnailLoadsInFlight.erase(filepath);
            return;
        }

        const std::filesystem::path relativePath = project->GetProjectRelativeFilepath(filepath);
        AssetHandle handle = m_AssetManager->GetAssetHandle(relativePath);
        if (handle == AssetHandle(0))
        {
            handle = m_AssetManager->GetAssetHandle(filepath);
        }

        if (handle == AssetHandle(0))
        {
            s_SharedThumbnails.erase(filepath);
            s_SharedThumbnailLoadsInFlight.erase(filepath);
            return;
        }

        const uint64_t panelGeneration = s_SharedThumbnailLoadGeneration;
        const std::filesystem::path capturedPath = filepath;
        const auto thumbnailSize = static_cast<uint32_t>(std::max(16, m_ThumbnailSize));

        ThumbnailRequest req{};
        req.handle = handle;
        req.type = type;
        req.assetPath = capturedPath;
        req.cachePath = ThumbnailGenerator::GetCacheFilepath(project.get(), handle, capturedPath);
        req.size = thumbnailSize;
        req.sourceTimestamp = ThumbnailGenerator::GetFileTimestamp(capturedPath);
        req.generation = ThumbnailGenerator::Get().GetGeneration();
        req.onComplete = [capturedPath, panelGeneration](Ref<Texture> loadedTexture)
        {
            if (panelGeneration != s_SharedThumbnailLoadGeneration)
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

            if (!loadedTexture)
            {
                s_SharedThumbnails.erase(thumbnailIt);
                s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                return;
            }

            FileThumbnail ft;
            ft.thumbnail = std::move(loadedTexture);
            ft.lastFrameUsed = s_SharedCurrentFrame;
            ft.timestamp = static_cast<uint64_t>(ThumbnailGenerator::GetFileTimestamp(capturedPath));

            thumbnailIt->second = std::move(ft);
            s_SharedThumbnailLoadsInFlight.erase(capturedPath);
        };

        ThumbnailGenerator::Get().RequestThumbnail(std::move(req));
    }

    void ContentBrowserPanel::InvalidateThumbnailForAsset(const AssetChangeSignal &signal)
    {
        if (signal.handle == AssetHandle(0))
            return;

        AssetManager *assetManager = AssetManager::GetInstance();
        if (!assetManager)
            return;

        Ref<Project> project = assetManager->LockActiveProject();
        if (!project)
            return;

        auto invalidateHandle = [&project, assetManager](AssetHandle handle)
        {
            if (handle == AssetHandle(0))
                return;

            const AssetMetaData meta = assetManager->GetMetaData(handle);
            if (meta.filepath.empty())
                return;

            const std::filesystem::path fullPath = project->GetProjectFilepath(meta.filepath).lexically_normal();
            if (ThumbnailGenerator::IsSupportedAssetType(meta.type))
            {
                ThumbnailGenerator::Get().InvalidateThumbnail(project.get(), handle, fullPath);
            }

            const std::string targetNorm = stringutils::ToLower(fullPath.generic_string());
            Application::SubmitToRenderThread([targetNorm]()
            {
                for (auto it = s_SharedThumbnails.begin(); it != s_SharedThumbnails.end();)
                {
                    if (stringutils::ToLower(it->first.lexically_normal().generic_string()) == targetNorm)
                    {
                        s_SharedThumbnailLoadsInFlight.erase(it->first);
                        it = s_SharedThumbnails.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }, "ContentBrowserPanel::InvalidateThumbnail");
        };

        std::unordered_set<AssetHandle> handlesToInvalidate;
        if (signal.type == AssetType::Texture || ThumbnailGenerator::IsSupportedAssetType(signal.type))
        {
            handlesToInvalidate.insert(signal.handle);
        }

        // Cascade texture -> materials -> meshes, and material -> meshes so dependent thumbnails update too.
        std::unordered_set<AssetHandle> affectedMaterials;
        if (signal.type == AssetType::Material)
        {
            affectedMaterials.insert(signal.handle);
        }
        else if (signal.type == AssetType::Texture)
        {
            for (const auto &[matHandle, asset] : assetManager->GetLoadedAssets())
            {
                if (!asset || asset->GetAssetType() != AssetType::Material)
                    continue;
                Ref<Material> mat = std::static_pointer_cast<Material>(asset);
                if (mat && (mat->baseColorTextureHandle == signal.handle
                    || mat->emissiveTextureHandle == signal.handle
                    || mat->metallicTextureHandle == signal.handle
                    || mat->roughnessTextureHandle == signal.handle
                    || mat->normalTextureHandle == signal.handle
                    || mat->occlusionTextureHandle == signal.handle))
                {
                    affectedMaterials.insert(matHandle);
                    handlesToInvalidate.insert(matHandle);
                }
            }
        }

        if (!affectedMaterials.empty())
        {
            for (const auto &[meshHandle, asset] : assetManager->GetLoadedAssets())
            {
                if (!asset)
                    continue;

                if (asset->GetAssetType() == AssetType::StaticMesh)
                {
                    Ref<StaticMesh> mesh = std::static_pointer_cast<StaticMesh>(asset);
                    for (const auto &inst : mesh->GetMeshInstances())
                    {
                        if (inst && affectedMaterials.contains(inst->GetMaterialAssetHandle()))
                        {
                            handlesToInvalidate.insert(meshHandle);
                            break;
                        }
                    }
                }
                else if (asset->GetAssetType() == AssetType::SkeletalMesh)
                {
                    Ref<SkeletalMesh> mesh = std::static_pointer_cast<SkeletalMesh>(asset);
                    for (const auto &inst : mesh->GetMeshInstances())
                    {
                        if (inst && affectedMaterials.contains(inst->GetMaterialAssetHandle()))
                        {
                            handlesToInvalidate.insert(meshHandle);
                            break;
                        }
                    }
                }
            }
        }

        for (AssetHandle handle : handlesToInvalidate)
        {
            invalidateHandle(handle);
        }
    }

    void ContentBrowserPanel::StartThumbnailLoad(const std::filesystem::path &filepath)
    {
        IGN_PROFILE_FUNCTION();

        // Capture by value to avoid dangling references
        std::filesystem::path capturedPath = filepath;
        int thumbnailSize = m_ThumbnailSize;
        const uint64_t requestGeneration = s_SharedThumbnailLoadGeneration;

        AssetWorker::SubmitBackgroundJob([this, capturedPath, thumbnailSize, requestGeneration]()
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
                nri::Format format = nri::Format::RGBA8_UNORM;
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

                const uint64_t resizedSize = static_cast<uint64_t>(destWidth) * destHeight * kChannels;
                destPixels.resize(resizedSize);
                texture_utils::DownSample(srcPixels.data(), destPixels.data(), srcWidth, srcHeight, destWidth, destHeight, kChannels);
                stbi_image_free(pixelData);
                pixelData = nullptr;
            }

            // -----------------------------------------------------------------------
            // WORKER THREAD: Prepare deferred Texture + CPU mip chain, then queue
            // non-blocking batched GPU upload on the render thread.
            // -----------------------------------------------------------------------
            if (requestGeneration != s_SharedThumbnailLoadGeneration || destPixels.empty())
            {
                Application::SubmitToRenderThread([capturedPath]()
                {
                    s_SharedThumbnails.erase(capturedPath);
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                }, "ContentBrowserPanel::StartThumbnailLoad - Drop stale thumbnail");
                return;
            }

            TextureCreateInfo createInfo;
            createInfo.format = nri::Format::RGBA8_UNORM;
            createInfo.keepInitialState = true;
            createInfo.keepCpuData = false;
            createInfo.deferGpuCreate = true;
            createInfo.bindless = false;
            createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
            createInfo.initialAccess = nri::AccessBits::SHADER_RESOURCE;
            createInfo.width = static_cast<uint32_t>(destWidth);
            createInfo.height = static_cast<uint32_t>(destHeight);
            createInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
            createInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
            createInfo.samplerAddressW = nri::AddressMode::CLAMP_TO_EDGE;
            createInfo.samplerLinearFiltering = true;

            Ref<Texture> loadedTexture = Texture::Create(destPixels, createInfo, nullptr);
            if (!loadedTexture)
            {
                Application::SubmitToRenderThread([capturedPath]()
                {
                    s_SharedThumbnails.erase(capturedPath);
                    s_SharedThumbnailLoadsInFlight.erase(capturedPath);
                }, "ContentBrowserPanel::StartThumbnailLoad - Cleanup failed thumbnail");
                return;
            }

            loadedTexture->PrepareUploadData(4);

            Texture::SubmitAsyncUpload(loadedTexture, [capturedPath, requestGeneration, loadedTexture]()
            {
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

                thumbnailIt->second = ft;
                s_SharedThumbnailLoadsInFlight.erase(capturedPath);
            });
        });
    }

    void ContentBrowserPanel::UnloadUnusedThumbnails()
    {
        IGN_PROFILE_FUNCTION();

        std::vector<std::filesystem::path> toUnload;

        for (const auto& [path, thumbnail] : s_SharedThumbnails)
        {
            const bool isStale = (s_SharedCurrentFrame - thumbnail.lastFrameUsed) > s_ThumbnailUnloadFrameThreshold;
            if (isStale && (thumbnail.thumbnail || !s_SharedThumbnailLoadsInFlight.contains(path)))
            {
                toUnload.push_back(path);
            }
        }

        for (const auto& path : toUnload)
        {
            s_SharedThumbnailLoadsInFlight.erase(path);
            s_SharedThumbnails.erase(path);
        }
    }

    void ContentBrowserPanel::ClearThumbnails()
    {
        s_SharedThumbnailLoadGeneration++;
        ThumbnailGenerator::Get().ClearPending();
        s_SharedThumbnails.clear();
        s_SharedThumbnailLoadsInFlight.clear();

        while (!s_SharedPendingThumbnailLoads.empty())
        {
            s_SharedPendingThumbnailLoads.pop();
        }
    }
}
