//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#include "content_browser_panel.hpp"
#include "ignite/project/project.hpp"
#include "editor_layer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/scene/sprite_sheet.hpp"

#include "ignite/core/input/asset_import_event.hpp"
#include "ignite/core/input/app_event.hpp"

#include <format>
#include <algorithm>
#include <ranges>
#include <cstring>
#include <SDL3/SDL_dialog.h>

namespace ignite
{
    namespace
    {
        static void DispatchOpenAssetEditorEvent(AssetHandle handle, const AssetMetaData &metadata)
        {
            if (handle == AssetHandle(0) || metadata.type == AssetType::Invalid)
            {
                return;
            }

            Application::SubmitToMainThread([handle, metadata]()
            {
                AssetEditorOpenEvent openEvent(handle, metadata);
                Application::GetInstance()->OnEvent(openEvent);
            });
        }

        static void DispatchCreateAssetEditorEvent(AssetType assetType, const std::filesystem::path &targetDirectory)
        {
            if (assetType == AssetType::Invalid)
            {
                return;
            }

            Application::SubmitToMainThread([assetType, targetDirectory]()
            {
                AssetEditorCreateEvent createEvent(assetType, targetDirectory);
                Application::GetInstance()->OnEvent(createEvent);
            });
        }

        static const SDL_DialogFileFilter kSkeletalMeshFilters[]
        {
            {"FBX File (.fbx)", "fbx"},
            {"Ignite Skeletal Mesh (.ixskm)", "ixskm"}
        };

        static const SDL_DialogFileFilter kStaticMeshFilters[]
        {
            {"GLTF File (.gltf)", "gltf"},
            {"FBX File (.fbx)", "fbx"},
            {"Ignite Static Mesh (.ixsm)", "ixsm"}
        };

        static const SDL_DialogFileFilter kTextureFilters[]
        {
            {"Texture (.png)", "png"},
            {"Texture (.jpg)", "jpg"},
            {"Texture (.jpeg)", "jpeg"},
            {"Texture (.hdr)", "hdr"}
        };

        static const SDL_DialogFileFilter kAudioFilters[]
        {
            {"Audio (.wav)", "wav"},
            {"Audio (.mp3)", "mp3"},
            {"Audio (.flac)", "flac"}
        };

        static const SDL_DialogFileFilter kFontFilters[]
        {
            {"Font (.ttf)", "ttf"},
            {"Font (.otf)", "otf"}
        };

        static const SDL_DialogFileFilter kSceneFilters[]
        {
            {"Ignite Scene (.ixscene)", "ixscene"}
        };

        static const SDL_DialogFileFilter kMaterialFilters[]
        {
            {"Ignite Material (.ixmat)", "ixmat"}
        };

        static const SDL_DialogFileFilter kMaterial2DFilters[]
        {
            {"Ignite Material2D (.ixmat2d)", "ixmat2d"}
        };
    }

    ContentBrowserPanel::ContentBrowserPanel(const char *windowTitle, EditorLayer *editor)
        : IPanel(windowTitle, editor)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        
        auto app = Application::GetInstance();

        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        TextureCreateInfo createInfo;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
    	createInfo.keepInitialState = true;
    	createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        m_Icons["folder"] = Texture::Create("resources/ui/ic_folder.png", createInfo, cmd);
        m_Icons["unknown"] = Texture::Create("resources/ui/ic_file.png", createInfo, cmd);

        cmd->close();
        app->SubmitWorkerCommandList(cmd);

        m_AssetEditorPanel = new AssetEditorPanel("Animation Panel", editor);
        app->PushLayer(m_AssetEditorPanel);
    }

	ContentBrowserPanel::~ContentBrowserPanel()
	{
	}

	void ContentBrowserPanel::LoadProjectFiles()
    {
        // clear directories
        m_PathEntryList.clear();
        m_TreeNodes.clear();

        m_TreeNodes.emplace_back(".", AssetHandle(0));
        
        m_BaseDirectory = Project::GetInstance()->GetAssetDirectory();
        m_PathEntryList.push_back(m_BaseDirectory);

        m_CurrentDirectory = Project::GetInstance()->GetAssetDirectory();
        RefreshAssetTree();
    }

    void ContentBrowserPanel::RenderFileTree(FileTreeNode *node)
    {
        if (node->path.empty())
            return;

        // Get the node's index in the tree
        uint32_t nodeIndex = static_cast<uint32_t>(node - m_TreeNodes.data());
        
        // Build full path using helper function
        const std::filesystem::path assetDir = Project::GetInstance()->GetAssetDirectory();
        const std::filesystem::path relativePath = GetNodeFullpath(nodeIndex);
        const std::filesystem::path fullPath = assetDir / relativePath;
        const std::string filename = node->path.filename().string();
        
        // Check if path exists and is a directory
        bool isDirectory = false;
        if (std::filesystem::exists(fullPath))
        {
            isDirectory = std::filesystem::is_directory(fullPath);
        }

        ImGuiTreeNodeFlags flags = (m_SelectedFileTree == fullPath ? ImGuiTreeNodeFlags_Selected : 0)
            | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth;

        if (!isDirectory)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool opened = ImGui::TreeNodeEx(filename.c_str(), flags, "%s", filename.c_str());

        DragDropSource(fullPath);

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (isDirectory)
            {
                m_BackwardPathStack.push(m_CurrentDirectory);
                m_SelectedFileTree = m_CurrentDirectory = fullPath;
            }
            else
            {
                auto *project = m_EditorLayer->GetActiveProject().get();
                if (!project)
                    return;

                const std::filesystem::path relativeAssetPath = project->GetAssetRelativeFilepath(fullPath);
                auto &assetManager = project->GetAssetManager();
                AssetHandle handle = assetManager.GetAssetHandle(relativeAssetPath);
                AssetMetaData metadata = assetManager.GetMetaData(handle);
                DispatchOpenAssetEditorEvent(handle, metadata);
            }
        }

        if (opened && isDirectory)
        {
            std::vector<uint32_t> sortedChildNodeIndices;
            sortedChildNodeIndices.reserve(node->children.size());

            for (const auto &childNodeIndex : node->children | std::views::values)
            {
                if (childNodeIndex < m_TreeNodes.size())
                {
                    sortedChildNodeIndices.push_back(childNodeIndex);
                }
            }

            std::ranges::sort(sortedChildNodeIndices, [this, &assetDir](uint32_t leftIndex, uint32_t rightIndex)
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

            for (const auto &childNodeIndex : sortedChildNodeIndices)
            {
                RenderFileTree(&m_TreeNodes[childNodeIndex]);
            }
            
            ImGui::TreePop();
        }
    }

    void ContentBrowserPanel::OnGuiRender()
    {
        if (ImGui::Begin("Content Browser"))
        {
            const float &dpiScale = ImGui::GetWindowDpiScale();
            const auto navbarBtSize = ImVec2(40.0f * dpiScale, 24.0f * dpiScale);

            // Calculate navbar height based on button size + padding
            const ImGuiStyle &style = ImGui::GetStyle();
            const float navbarHeight = navbarBtSize.y + style.FramePadding.y * 2.0f + style.WindowPadding.y * 2.0f;

            // Navigation bar
            ImGui::BeginChild("##NAV_BUTTON_BAR", ImVec2(0, navbarHeight), ImGuiChildFlags_Borders);

            if (ImGui::Button("<-", navbarBtSize))
            {
                if (!m_BackwardPathStack.empty())
                {
                    m_ForwardPathStack.push(m_CurrentDirectory);
                    m_CurrentDirectory = m_BackwardPathStack.top();
                    m_BackwardPathStack.pop();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("->", navbarBtSize))
            {
                if (!m_ForwardPathStack.empty())
                {
                    m_BackwardPathStack.push(m_CurrentDirectory);
                    m_CurrentDirectory = m_ForwardPathStack.top();
                    m_ForwardPathStack.pop();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("R", navbarBtSize))
            {
                Project::GetInstance()->ValidateAssetRegistry();
                PruneMissingNodes(0, Project::GetInstance()->GetAssetDirectory());
                RefreshAssetTree();
                CompactTree();
            }

            ImGui::SameLine();
            if (ImGui::Button("+Import", ImVec2(80.0f * dpiScale, navbarBtSize.y)))
            {
                ImGui::OpenPopup("##asset_importer_context");
            }

            if (ImGui::BeginPopupContextWindow("##asset_importer_context"))
            {
                UIShowAssetImportContext();
                ImGui::EndPopup();
            }
                
            ImGui::EndChild();

            if (Project::GetInstance())
            {
                // Left side directory tree
                ImGui::BeginChild("left_item_browser", { 300.0f, 0.0f }, ImGuiChildFlags_ResizeX);
                if (!m_TreeNodes.empty())
                {
                    const std::filesystem::path assetDir = Project::GetInstance()->GetAssetDirectory();
                    std::vector<uint32_t> sortedRootNodeIndices;
                    sortedRootNodeIndices.reserve(m_TreeNodes.size() - 1);

                    for (uint32_t i = 1; i < m_TreeNodes.size(); ++i)
                    {
                        if (m_TreeNodes[i].parent == 0)
                        {
                            sortedRootNodeIndices.push_back(i);
                        }
                    }

                    std::ranges::sort(sortedRootNodeIndices, [this, &assetDir](uint32_t leftIndex, uint32_t rightIndex)
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

                    for (const uint32_t rootNodeIndex : sortedRootNodeIndices)
                    {
                        RenderFileTree(&m_TreeNodes[rootNodeIndex]);
                    }
                }
                ImGui::EndChild();
                ImGui::SameLine();

                // Files
                ImGui::BeginChild("##FILE_LISTS", { 0.0f, 0.0f });

                // Insert path nodes
                FileTreeNode *node = m_TreeNodes.data();
                if (node)
                {
                    auto f = Project::GetInstance()->GetAssetDirectory();
                    const auto &relativePath = std::filesystem::relative(m_CurrentDirectory, f);

                    for (const auto &path : relativePath)
                    {
                        if (node->path == relativePath)
                        {
                            break;
                        }

                        if (node->children.contains(path))
                        {
                            node = &m_TreeNodes[node->children[path]];
                        }
                    }

                    if (node->children.empty())
                    {
                        ImGui::Text("This folder is empty");
                    }

                    static float padding = 12.0f;
                    const float cellSize = static_cast<float>(m_ThumbnailSize) + padding;
                    const float &childWindowWidth = ImGui::GetContentRegionAvail().x;

                    int columnCount = static_cast<int>(childWindowWidth / cellSize);
                    columnCount = std::max(columnCount, 1);

                    ImGui::Columns(columnCount, nullptr, false);

                    std::vector<std::filesystem::path> sortedItems;
                    sortedItems.reserve(node->children.size());
                    for (const auto &item : node->children | std::views::keys)
                    {
                        sortedItems.push_back(item);
                    }

                    std::ranges::sort(sortedItems, [this](const std::filesystem::path &left, const std::filesystem::path &right)
                    {
                        const bool leftIsDirectory = std::filesystem::is_directory(m_CurrentDirectory / left);
                        const bool rightIsDirectory = std::filesystem::is_directory(m_CurrentDirectory / right);

                        if (leftIsDirectory != rightIsDirectory)
                        {
                            return leftIsDirectory;
                        }

                        return left.generic_string() < right.generic_string();
                    });

                    for (const auto &item : sortedItems)
                    {
                        std::string filenameStr = item.generic_string();
                        ImGui::PushID(filenameStr.c_str());

                        std::filesystem::path path = m_CurrentDirectory / item;
                        bool isDirectory = std::filesystem::is_directory(path);

                        Ref<Texture> icon;
                        if (isDirectory)
                        {
                            icon = m_Icons["folder"];
                        }
                        else if (IsImageFile(path))
                        {
                            icon = GetOrCreateThumbnail(path);
                            if (!icon)
                                icon = m_Icons["unknown"];
                        }
                        else
                        {
                            icon = m_Icons["unknown"];
                        }

                        // Keep a fixed clickable area and draw the image separately with preserved aspect ratio
                        const float maxSize = static_cast<float>(m_ThumbnailSize);
                        const ImVec2 buttonSize(maxSize, maxSize);
                        const ImVec2 buttonMin = ImGui::GetCursorScreenPos();
                        const ImVec2 buttonMax(buttonMin.x + buttonSize.x, buttonMin.y + buttonSize.y);

                        ImGui::InvisibleButton(item.string().c_str(), buttonSize);

                        const bool isActive = ImGui::IsItemActive();
                        const bool isHovered = ImGui::IsItemHovered();
                        const ImU32 buttonColor = isActive
                            ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                            : (isHovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : ImGui::GetColorU32(ImGuiCol_Button));
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            buttonMin,
                            buttonMax,
                            buttonColor,
                            ImGui::GetStyle().FrameRounding);

                        const ImVec2 displaySize = CalculateThumbnailDisplaySize(icon, maxSize);
                        const ImVec2 imageMin(
                            buttonMin.x + (maxSize - displaySize.x) * 0.5f,
                            buttonMin.y + (maxSize - displaySize.y) * 0.5f);
                        const ImVec2 imageMax(imageMin.x + displaySize.x, imageMin.y + displaySize.y);

                        ImTextureID iconId = reinterpret_cast<ImTextureID>(icon->GetHandle().Get());
                        ImGui::GetWindowDrawList()->AddImage(iconId, imageMin, imageMax);

                        if (ImGui::IsItemHovered())
                        {
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                if (isDirectory)
                                {
                                    m_BackwardPathStack.push(m_CurrentDirectory);
                                    m_CurrentDirectory = path;
                                }
                                else if (m_EditorLayer && m_EditorLayer->GetActiveProject())
                                {
                                    auto *project = m_EditorLayer->GetActiveProject().get();
                                    const std::filesystem::path relativeAssetPath = project->GetAssetRelativeFilepath(path);
                                    auto &assetManager = project->GetAssetManager();

                                    AssetHandle handle = assetManager.GetAssetHandle(relativeAssetPath);
                                    AssetMetaData metadata = assetManager.GetMetaData(handle);
                                    DispatchOpenAssetEditorEvent(handle, metadata);
                                }
                            }
                        }

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (!isDirectory && ImGui::MenuItem("Open in Asset Editor"))
                            {
                                if (m_EditorLayer && m_EditorLayer->GetActiveProject())
                                {
                                    auto *project = m_EditorLayer->GetActiveProject().get();
                                    const std::filesystem::path relativeAssetPath = project->GetAssetRelativeFilepath(path);
                                    auto &assetManager = project->GetAssetManager();

                                    AssetHandle handle = assetManager.GetAssetHandle(relativeAssetPath);
                                    AssetMetaData metadata = assetManager.GetMetaData(handle);
                                    DispatchOpenAssetEditorEvent(handle, metadata);
                                }
                            }
                            // Rename
                            if (ImGui::MenuItem("Rename"))
                            {
                                m_PopupTargetPath = path;
                                // prefill buffer with filename
                                std::string fname = path.filename().generic_string();
                                std::strncpy(m_PopupInputBuffer, fname.c_str(), sizeof(m_PopupInputBuffer) - 1);
                                m_ShowRenameModal = true;
                            }

                            // Delete
                            if (ImGui::MenuItem("Delete"))
                            {
                                m_PopupTargetPath = path;
                                m_ShowDeleteModal = true;
                            }

                            if (ImGui::MenuItem("Open"))
                            {
                                if (isDirectory)
                                {
                                    m_BackwardPathStack.push(m_CurrentDirectory);
                                    m_CurrentDirectory = path;
                                }
                                else
                                {
                                    // Windows
                                    std::string command = std::format("\"{}\"", path.generic_string());
                                    std::system(command.c_str());
                                }
                            }

                            if (ImGui::MenuItem("Import"))
                            {
                                Project::GetInstance()->GetAssetManager().ImportAsset(path);
                            }

                            if (item.extension() == ".ixscene")
                            {
                                if (ImGui::MenuItem("Set As Default Scene"))
                                {
                                    auto project = m_EditorLayer->GetActiveProject();
                                    if (project)
                                    {
                                        project->GetAssetManager().ImportAsset(path);
									    AssetHandle handle = project->GetAssetManager().GetAssetHandle(path);
                                        project->SetDefaultScene(handle);

                                        project->Serialize(project->GetFilepath());
                                    }
                                }
                            }

                            ImGui::Separator();
                            ImGui::Text("%s", filenameStr.c_str());

                            ImGui::EndPopup();
                        }

                        DragDropSource(m_CurrentDirectory / item);
                        ImGui::TextWrapped("%s", filenameStr.c_str());

                        if (!isDirectory && m_EditorLayer && m_EditorLayer->GetActiveProject())
                        {
                            Project *project = m_EditorLayer->GetActiveProject().get();
                            auto &assetManager = project->GetAssetManager();
                            const std::filesystem::path relativeAssetPath = project->GetAssetRelativeFilepath(path);
                            AssetHandle handle = assetManager.GetAssetHandle(relativeAssetPath);
                            AssetMetaData metadata = assetManager.GetMetaData(handle);

                            if (metadata.type == AssetType::SpriteSheet && handle != AssetHandle(0))
                            {
                                const std::string popupId = std::format("##sprites_popup_{}", filenameStr);
                                if (ImGui::SmallButton(std::format("Sprites##{}", filenameStr).c_str()))
                                {
                                    ImGui::OpenPopup(popupId.c_str());
                                }

                                if (ImGui::BeginPopup(popupId.c_str()))
                                {
                                    Ref<SpriteSheet> spriteSheet = project->GetAsset<SpriteSheet>(handle);
                                    if (!spriteSheet)
                                    {
                                        spriteSheet = project->GetAssetImmediate<SpriteSheet>(handle);
                                    }

                                    if (spriteSheet)
                                    {
                                        Ref<Texture> texture = nullptr;
                                        if (spriteSheet->GetTextureHandle() != AssetHandle(0))
                                        {
                                            texture = project->GetAsset<Texture>(spriteSheet->GetTextureHandle());
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

                        ImGui::NextColumn();
                        ImGui::PopID();
                    }
                }

                ImGui::Columns(1);

                if (ImGui::BeginPopupContextWindow("##content_browser_context_menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoReopen | ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (ImGui::BeginMenu("Create"))
                    {
                        if (ImGui::MenuItem("New Folder"))
                        {
                            // show create folder modal
                            m_ShowCreateFolderModal = true;
                        }

                        if (ImGui::MenuItem("Sprite Sheet"))
                        {
							DispatchCreateAssetEditorEvent(AssetType::SpriteSheet, m_CurrentDirectory);
                        }

                        if (ImGui::BeginMenu("Animation"))
                        {
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

                            ImGui::EndMenu();
                        }

                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Thumbnail Size"))
                    {
                        if (ImGui::MenuItem("Small")) m_ThumbnailSize = 38;
                        if (ImGui::MenuItem("Medium")) m_ThumbnailSize = 64;
                        if (ImGui::MenuItem("Large")) m_ThumbnailSize = 96;
                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem("Open Folder in File Explorer"))
                    {
                        std::string command = std::format("explorer {}", m_CurrentDirectory.string());
                        std::system(command.c_str());
                    }

                    ImGui::EndPopup();
                }

                ImGui::EndChild();
            }

            // Create Folder Modal
            if (m_ShowCreateFolderModal)
            {
                ImGui::OpenPopup("Create Folder");
                m_ShowCreateFolderModal = false;
            }

            if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Create folder in: %s", m_CurrentDirectory.generic_string().c_str());
                ImGui::Spacing();
                ImGui::InputText("Folder Name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer));

                ImGui::Separator();
                if (ImGui::Button("Create"))
                {
                    std::string name = m_PopupInputBuffer;
                    if (!name.empty())
                    {
                        std::filesystem::path newPath = m_CurrentDirectory / name;
                        if (!std::filesystem::exists(newPath))
                        {
                            std::error_code ec;
                            std::filesystem::create_directory(newPath, ec);
                            if (!ec)
                            {
                                m_NeedsRefresh = true;
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

            // Rename Modal
            if (m_ShowRenameModal)
            {
                ImGui::OpenPopup("Rename Item");
                m_ShowRenameModal = false;
            }

            if (ImGui::BeginPopupModal("Rename Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Rename: %s", m_PopupTargetPath.filename().generic_string().c_str());
                ImGui::Spacing();
                ImGui::InputText("New Name", m_PopupInputBuffer, sizeof(m_PopupInputBuffer));

                ImGui::Separator();
                if (ImGui::Button("Rename"))
                {
                    std::string newName = m_PopupInputBuffer;
                    if (!newName.empty())
                    {
                        std::filesystem::path target = m_PopupTargetPath;
                        std::filesystem::path dest = target.parent_path() / newName;
                        std::error_code ec;
                        std::filesystem::rename(target, dest, ec);
                        if (!ec)
                        {
                            m_NeedsRefresh = true;
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

            // Delete Confirmation Modal
            if (m_ShowDeleteModal)
            {
                ImGui::OpenPopup("Delete Item");
                m_ShowDeleteModal = false;
            }

            if (ImGui::BeginPopupModal("Delete Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Are you sure you want to delete: %s ?", m_PopupTargetPath.filename().generic_string().c_str());
                ImGui::Spacing();
                ImGui::Separator();
                if (ImGui::Button("Delete"))
                {
                    std::error_code ec;
                    if (std::filesystem::is_directory(m_PopupTargetPath))
                        std::filesystem::remove_all(m_PopupTargetPath, ec);
                    else
                        std::filesystem::remove(m_PopupTargetPath, ec);

                    if (!ec)
                        m_NeedsRefresh = true;

                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }
    }

	void ContentBrowserPanel::OnUpdate(float deltaTime)
	{
        // Increment frame counter
        m_CurrentFrame++;

        while (!m_PendingAssetLoading.empty())
        {
            auto [assetType, assetMetaData, _] = m_PendingAssetLoading.front();
			m_PendingAssetLoading.pop();

            if (assetType == PendingFileLoading::ImportAssets)
            {
                Project::GetInstance()->GetAssetManager().SubmitJob([this, assetType, assetMetaData]()
                {
					Ref<Asset> asset = Project::GetInstance()->GetAssetManager().Import(AssetHandle(), assetMetaData);

					if (asset)
					{
                        Application::SubmitToMainThread([this]() mutable
                        {
							m_NeedsRefresh = true;
                        });
					}
                });
            }
        }

        // Perform refresh once per frame if needed, avoiding overlapping command lists
        if (m_NeedsRefresh)
        {
            m_NeedsRefresh = false;
            m_EditorLayer->GetActiveProject()->ValidateAssetRegistry();
            PruneMissingNodes(0, m_EditorLayer->GetActiveProject()->GetAssetDirectory());
            RefreshAssetTree();
            CompactTree();

            auto f = Project::GetInstance()->GetFilepath();
            m_EditorLayer->GetActiveProject()->Serialize(f);
        }

        // Check if thumbnail size changed and clear thumbnails if needed
        if (m_ThumbnailSize != m_LastThumbnailSize)
        {
            ClearThumbnails();
            m_LastThumbnailSize = m_ThumbnailSize;
        }

        // Load only 1 thumbnail per frame
        if (!m_PendingThumbnailLoads.empty() && m_CurrentFrame % 3 == 0)
        {
            std::filesystem::path filepath = m_PendingThumbnailLoads.front();
            m_PendingThumbnailLoads.pop();
            
            // Check if still needs loading (not already loaded)
            auto it = m_Thumbnails.find(filepath);
            if (it != m_Thumbnails.end() && it->second.thumbnail == nullptr)
            {
                StartThumbnailLoad(filepath);
            }
        }

        // Unload unused thumbnails every 60 frames (once per second at 60fps)
        if (m_CurrentFrame % 60 == 0)
        {
            UnloadUnusedThumbnails();
        }

	}

	void ContentBrowserPanel::RefreshEntryPathList()
    {
        if (!m_PathEntryList.empty())
        {
            m_PathEntryList.erase(m_PathEntryList.begin() + 1, m_PathEntryList.end());
        }

        const auto &relativePath = std::filesystem::relative(m_CurrentDirectory, Project::GetInstance()->GetAssetDirectory());
        auto currentDir = Project::GetInstance()->GetAssetDirectory();

        for (const auto &p : relativePath)
        {
            const std::string &pString = p.string();
            if (pString != ".")
            {
                currentDir /= p;
                m_PathEntryList.push_back(currentDir);
            }
        }
    }

    void ContentBrowserPanel::RefreshAssetTree()
    {
        const std::filesystem::path &assetPath = Project::GetInstance()->GetAssetDirectory();
        LoadAssetTree(assetPath);
    }

    void ContentBrowserPanel::LoadAssetTree(const std::filesystem::path &directory)
    {
        const std::filesystem::path assetPath = Project::GetInstance()->GetAssetDirectory();

        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            const std::filesystem::path &relativePath = std::filesystem::relative(entry.path(), assetPath);
            uint32_t currentNodeIndex = 0;
            
            for (const std::filesystem::path &path : relativePath)
            {
                const auto it = m_TreeNodes[currentNodeIndex].children.find(path.generic_string());
                if (it != m_TreeNodes[currentNodeIndex].children.end())
                {
                    currentNodeIndex = it->second;
                }
                else
                {
                    AssetHandle assetHandle = AssetHandle(0);
                    if (!std::filesystem::is_directory(path) && path.has_extension())
                    {
                        std::string relPath = relativePath.generic_string();
                        assetHandle = Project::GetInstance()->GetAssetManager().GetAssetHandle(relPath);
                        AssetType assetType = GetAssetTypeFromExtension(relativePath.extension().generic_string());
                        
                        // not registered yet
                        // (insert the metadata and generate the asset handle)
                        if (assetHandle == AssetHandle(0))
                        {
                            assetHandle = AssetHandle();
                            AssetMetaData metadata;
                            metadata.type = assetType;
                            metadata.filepath = relPath;
                            Project::GetInstance()->GetAssetManager().AssignMetaData(assetHandle, metadata);
                        }

                        if (assetType == AssetType::Material)
                        {
                            // Project::GetInstance()->GetAsset<Material>(assetHandle);
                        }
                    }

                    FileTreeNode newNode(path, assetHandle);
                    newNode.parent = currentNodeIndex;

                    m_TreeNodes.push_back(newNode);
                    m_TreeNodes[currentNodeIndex].children[path] = static_cast<int>(m_TreeNodes.size()) - 1;
                    currentNodeIndex = static_cast<int>(m_TreeNodes.size()) - 1;
                }
            }
            
            if (entry.is_directory())
            {
                LoadAssetTree(entry.path());
            }
        }
    }

	void ContentBrowserPanel::UIShowAssetImportContext()
	{
       if (ImGui::BeginMenu("Mesh"))
        {
            if (ImGui::MenuItem("Skeletal Mesh"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kSkeletalMeshFilters, IM_ARRAYSIZE(kSkeletalMeshFilters),
                    nullptr, true);
            }

            if (ImGui::MenuItem("Static Mesh"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kStaticMeshFilters, IM_ARRAYSIZE(kStaticMeshFilters),
                    nullptr, true);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Texture"))
        {
            if (ImGui::MenuItem("2D Texture"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kTextureFilters, IM_ARRAYSIZE(kTextureFilters),
                    nullptr, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Font"))
        {
            if (ImGui::MenuItem("MSDF Font"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kFontFilters, IM_ARRAYSIZE(kFontFilters),
                    nullptr, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Audio"))
        {
            if (ImGui::MenuItem("Sound"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kAudioFilters, IM_ARRAYSIZE(kAudioFilters),
                    nullptr, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("Ignite Scene"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kSceneFilters, IM_ARRAYSIZE(kSceneFilters),
                    nullptr, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Material"))
        {
            if (ImGui::MenuItem("Ignite Material"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kMaterialFilters, IM_ARRAYSIZE(kMaterialFilters),
                    nullptr, true);
            }

            if (ImGui::MenuItem("Ignite Material2D"))
            {
                SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
                    Application::GetInstance()->GetWindow()->GetWindowHandle(),
                    kMaterial2DFilters, IM_ARRAYSIZE(kMaterial2DFilters),
                    nullptr, true);
            }
            ImGui::EndMenu();
        }
	}

	void ContentBrowserPanel::PruneMissingNodes(uint32_t nodeIndex, const std::filesystem::path &basePath)
    {
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

        for (const auto& name : toRemove)
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
        FileTreeNode &node = m_TreeNodes[nodeIndex];

        for (auto& childIndex : node.children | std::views::values)
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
        FileTreeNode &node = m_TreeNodes[nodeIndex];

        for (auto& childIndex : node.children | std::views::values)
        {
            CollectNodeAndDescendants(childIndex, nodesToDelete);
        }

        nodesToDelete.push_back(nodeIndex);
    }

    void ContentBrowserPanel::MarkNodeDeletedRecursive(uint32_t nodeIndex)
    {
        if (nodeIndex >= m_TreeNodes.size() || m_TreeNodes[nodeIndex].isDeleted)
            return;

        FileTreeNode &node = m_TreeNodes[nodeIndex];
        node.isDeleted = true;

        for (auto& childIndex : node.children | std::views::values)
        {
            MarkNodeDeletedRecursive(childIndex);
        }

        node.children.clear();
    }

    void ContentBrowserPanel::DeleteSingleNode(uint32_t nodeIndex)
    {
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
        for (auto &node : m_TreeNodes)
        {
            // Update parent index
            if (node.parent > deletedIndex)
            {
                node.parent--;
            }

            // Update children indices
            for (auto& childIndex : node.children | std::views::values)
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
            for (auto& childIndex : node.children | std::views::values)
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
    }

	void ContentBrowserPanel::DragDropSource(const std::filesystem::path &filepath)
	{
		if (ImGui::BeginDragDropSource())
		{
			if (!std::filesystem::is_directory(filepath))
			{
                Project *project = Project::GetInstance();
                const std::filesystem::path relativeAssetPath = project ? project->GetAssetRelativeFilepath(filepath) : filepath;
                AssetHandle handle = project ? project->GetAssetManager().GetAssetHandle(relativeAssetPath) : AssetHandle(0);
				if (handle != AssetHandle(0))
				{
					ImGui::SetDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM, &handle, sizeof(AssetHandle));
				}
			}

			ImGui::EndDragDropSource();
		}
	}

	void ContentBrowserPanel::OnImportAssetDialog(void *userData, const char *const *filelist, int filter)
	{
        ContentBrowserPanel *cb = (ContentBrowserPanel *)userData;
        if (!cb)
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

        for (const char *const *file = filelist; *file != nullptr; file++)
        {
            std::filesystem::path filepath = std::string(*file);
            std::filesystem::path targetDirectory = cb->m_CurrentDirectory;

            Application::SubmitToMainThread([filepath, targetDirectory]()
            {
				AssetType assetType = GetAssetTypeFromExtension(filepath.extension().string());
                if (assetType == AssetType::Invalid)
                {
                    LOG_WARN("Import Asset Dialog: Unsupported asset extension '{}'", filepath.extension().string());
                    return;
                }

                AssetImportEvent importEvent({filepath}, assetType, targetDirectory);
                Application::GetInstance()->OnEvent(importEvent);
            });
        }
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
        if (!std::filesystem::exists(filepath))
            return false;

        std::string ext = filepath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || 
               ext == ".bmp" || ext == ".tga" || ext == ".hdr";
    }

    ImVec2 ContentBrowserPanel::CalculateThumbnailDisplaySize(Ref<Texture> texture, float maxSize) const
    {
        if (!texture)
            return ImVec2(maxSize, maxSize);

        float textureWidth = static_cast<float>(texture->GetWidth());
        float textureHeight = static_cast<float>(texture->GetHeight());
        
        if (textureWidth <= 0 || textureHeight <= 0)
            return ImVec2(maxSize, maxSize);

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

    Ref<Texture> ContentBrowserPanel::GetOrCreateThumbnail(const std::filesystem::path &filepath)
    {
        auto it = m_Thumbnails.find(filepath);
        if (it != m_Thumbnails.end())
        {
            // Mark as used this frame
            it->second.lastFrameUsed = m_CurrentFrame;

            if (!it->second.thumbnail && !m_ThumbnailLoadsInFlight.contains(filepath))
            {
                m_PendingThumbnailLoads.push(filepath);
                m_ThumbnailLoadsInFlight.insert(filepath);
            }

            return it->second.thumbnail;
        }

        // Create placeholder entry to prevent duplicate jobs
        FileThumbnail placeholder;
        placeholder.thumbnail = nullptr;
        placeholder.timestamp = 0;
        placeholder.lastFrameUsed = m_CurrentFrame;
        m_Thumbnails[filepath] = placeholder;

        // Add to loading queue instead of starting immediately
        if (!m_ThumbnailLoadsInFlight.contains(filepath))
        {
            m_PendingThumbnailLoads.push(filepath);
            m_ThumbnailLoadsInFlight.insert(filepath);
        }

        return nullptr;
    }

    void ContentBrowserPanel::StartThumbnailLoad(const std::filesystem::path &filepath)
    {
        // Capture by value to avoid dangling references
        std::filesystem::path capturedPath = filepath;
        int thumbnailSize = m_ThumbnailSize;
        const uint64_t requestGeneration = m_ThumbnailLoadGeneration;

        Project::GetInstance()->GetAssetManager().SubmitJob([this, capturedPath, thumbnailSize, requestGeneration]()
        {
            TextureCreateInfo createInfo;
            createInfo.format = nvrhi::Format::RGBA8_UNORM;
            createInfo.keepInitialState = true;
            createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
            createInfo.width = thumbnailSize;
            createInfo.height = thumbnailSize;
            createInfo.samplerAddressU = nvrhi::SamplerAddressMode::ClampToEdge;
            createInfo.samplerAddressV = nvrhi::SamplerAddressMode::ClampToEdge;
            createInfo.samplerAddressW = nvrhi::SamplerAddressMode::ClampToEdge;
            createInfo.samplerLinearFiltering = false;

            // Load texture data on worker thread (no command list yet)
            Ref<Texture> loadedTexture = Texture::Create(capturedPath.string().c_str(), createInfo, nullptr);

            // Submit to main thread to create command list and finalize GPU upload
            Application::SubmitToRenderThread([this, capturedPath, loadedTexture, requestGeneration]() mutable
            {
                if (requestGeneration != m_ThumbnailLoadGeneration)
                {
                    m_ThumbnailLoadsInFlight.erase(capturedPath);
                    return;
                }

                if (loadedTexture)
                {
                    nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                    nvrhi::CommandListHandle cmd = device->createCommandList();

                    {
                        std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
                        cmd->open();
                        cmd->beginMarker("Content browser thumbnails creation");
                        loadedTexture->SetData(cmd, 4);
                        cmd->endMarker();
                        cmd->close();
                    }
                    
                    Application::SubmitWorkerCommandList(cmd, [this, loadedTexture, capturedPath, requestGeneration]()
                    {
                        if (requestGeneration != m_ThumbnailLoadGeneration)
                        {
                            m_ThumbnailLoadsInFlight.erase(capturedPath);
                            return;
                        }

                        loadedTexture->SetReadyFlag(true);

						FileThumbnail ft;
						ft.thumbnail = loadedTexture;
						ft.lastFrameUsed = m_CurrentFrame;

						if (std::filesystem::exists(capturedPath))
						{
							ft.timestamp = std::filesystem::last_write_time(capturedPath).time_since_epoch().count();
						}
						else
						{
							ft.timestamp = 0;
						}

						m_Thumbnails[capturedPath] = ft;
                        m_ThumbnailLoadsInFlight.erase(capturedPath);
                    });
                }
                else
                {
                    // Remove placeholder if loading failed
                    m_Thumbnails.erase(capturedPath);
                    m_ThumbnailLoadsInFlight.erase(capturedPath);
                }
            });
        });
    }

    void ContentBrowserPanel::UnloadUnusedThumbnails()
    {
        std::vector<std::filesystem::path> toUnload;
        
        for (const auto& [path, thumbnail] : m_Thumbnails)
        {
            const bool isStale = (m_CurrentFrame - thumbnail.lastFrameUsed) > s_ThumbnailUnloadFrameThreshold;

            // Unload stale GPU thumbnails and also stale placeholders to prevent map growth.
            // Keep placeholders that are currently loading to avoid duplicate in-flight reloads.
            if (isStale && (thumbnail.thumbnail || !m_ThumbnailLoadsInFlight.contains(path)))
            {
                toUnload.push_back(path);
            }
        }
        
        // Unload the thumbnails
        for (const auto& path : toUnload)
        {
            m_Thumbnails.erase(path);
        }
    }

    void ContentBrowserPanel::ClearThumbnails()
    {
        m_ThumbnailLoadGeneration++;
        m_Thumbnails.clear();
        m_ThumbnailLoadsInFlight.clear();
        
        // Clear the pending load queue
        while (!m_PendingThumbnailLoads.empty())
        {
            m_PendingThumbnailLoads.pop();
        }
    }
}
