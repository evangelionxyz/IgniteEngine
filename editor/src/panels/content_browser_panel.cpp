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

#include "content_browser_panel.hpp"
#include "ignite/project/project.hpp"
#include "editor_layer.hpp"

#include <format>
#include <algorithm>
#include <ranges>

namespace ignite
{
    ContentBrowserPanel::ContentBrowserPanel(const char *windowTitle)
        : IPanel(windowTitle)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        TextureCreateInfo createInfo;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
    	createInfo.keepInitialState = true;
    	createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        m_Icons["folder"] = Texture::Create("resources/ui/ic_folder.png", createInfo, cmd);
        m_Icons["unknown"] = Texture::Create("resources/ui/ic_file.png", createInfo, cmd);

        cmd->close();
        Application::SubmitWorkerCommandList(cmd);
        // device->executeCommandList(cmd);
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

        if (ImGui::IsItemHovered())
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (isDirectory)
                {
                    m_BackwardPathStack.push(m_CurrentDirectory);
                    m_SelectedFileTree = m_CurrentDirectory = fullPath;
                }
            }
        }

        if (opened && isDirectory)
        {
            for (const auto &childNodeIndex : node->children | std::views::values)
            {
                if (childNodeIndex < m_TreeNodes.size())
                {
                    RenderFileTree(&m_TreeNodes[childNodeIndex]);
                }
            }
            
            ImGui::TreePop();
        }
    }

    void ContentBrowserPanel::OnGuiRender()
    {
        ImGui::Begin("Content Browser");

        const float &dpiScale = ImGui::GetWindowDpiScale();
        const auto navbarBtSize = ImVec2(40.0f * dpiScale, 24.0f * dpiScale);
        
        // Calculate navbar height based on button size + padding
        const ImGuiStyle& style = ImGui::GetStyle();
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
            static SDL_DialogFileFilter kFilters[]
            {
                {"All Files (*)", "*"}
            };

            bool allowMany = true;

			SDL_ShowOpenFileDialog(OnImportAssetDialog, this,
				Application::GetInstance()->GetWindow()->GetWindowHandle(),
				kFilters, IM_ARRAYSIZE(kFilters),
				nullptr, allowMany);
        }

        ImGui::EndChild();

        if (Project::GetInstance())
        {
            // Left side directory tree
            ImGui::BeginChild("left_item_browser", { 300.0f, 0.0f }, ImGuiChildFlags_ResizeX);
            for (auto it = m_TreeNodes.begin() + 1; it != m_TreeNodes.end(); ++it)
            {
                if (it->parent == 0)
                    RenderFileTree(&(*it));
            }
            ImGui::EndChild();
            ImGui::SameLine();

            // Files
            ImGui::BeginChild("##FILE_LISTS", { 0.0f, 0.0f });

            // Insert path nodes
            FileTreeNode *node = m_TreeNodes.data();
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

            for (const auto& item : node->children | std::views::keys)
            {
                std::string filenameStr = item.generic_string();
                ImGui::PushID(filenameStr.c_str());

                std::filesystem::path path = m_CurrentDirectory / item;
                bool isDirectory = std::filesystem::is_directory(path);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

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

                // Calculate display size with aspect ratio
                float maxSize = static_cast<float>(m_ThumbnailSize);
                ImVec2 displaySize = CalculateThumbnailDisplaySize(icon, maxSize);
                
                // Center the thumbnail vertically
                float diff = maxSize - displaySize.y;
                if (diff > 0)
                {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + diff * 0.5f);
                }
                
                ImTextureID iconId = reinterpret_cast<ImTextureID>( icon->GetHandle().Get());
                ImGui::ImageButton(item.string().c_str(), iconId, displaySize);

                if (ImGui::IsItemHovered())
                {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (isDirectory)
                        {
                            m_BackwardPathStack.push(m_CurrentDirectory);
                            m_CurrentDirectory = path;
                        }
                    }
                }

                if (ImGui::BeginPopupContextItem())
                {
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
                            Project::GetInstance()->GetAssetManager().ImportAsset(path);

                            AssetHandle handle = Project::GetInstance()->GetAssetManager().GetAssetHandle(path);
                            Project::GetInstance()->SetDefaultScene(handle);

                            ProjectSerializer serializer(Project::GetInstance());
                            serializer.Serialize(Project::GetInstance()->GetFilepath());
                        }
                    }

                    ImGui::Separator();
                    ImGui::Text("%s", filenameStr.c_str());

                    ImGui::EndPopup();
                }

                DragDropSource(m_CurrentDirectory / item);
                
                ImGui::PopStyleColor();
                ImGui::TextWrapped("%s", filenameStr.c_str());

                ImGui::NextColumn();
                ImGui::PopID();
            }

            ImGui::Columns(1);

            if (ImGui::BeginPopupContextWindow("##content_browser_context_menu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoReopen | ImGuiPopupFlags_NoOpenOverItems))
            {
                if (ImGui::BeginMenu("Create"))
                {
                    if (ImGui::MenuItem("New Folder"))
                    {
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

        ImGui::End();
    }

	void ContentBrowserPanel::OnUpdate(float deltaTime)
	{
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
                            return true;
                        });
					}
                });
            }
        }

        // Perform refresh once per frame if needed, avoiding overlapping command lists
        if (m_NeedsRefresh)
        {
            m_NeedsRefresh = false;
            Project::GetInstance()->ValidateAssetRegistry();
            PruneMissingNodes(0, Project::GetInstance()->GetAssetDirectory());
            RefreshAssetTree();
            CompactTree();
        }

        // Check if thumbnail size changed and clear thumbnails if needed
        if (m_ThumbnailSize != m_LastThumbnailSize)
        {
            ClearThumbnails();
            m_LastThumbnailSize = m_ThumbnailSize;
        }
	}

	void ContentBrowserPanel::RefreshEntryPathList()
    {
        m_PathEntryList.erase(m_PathEntryList.begin() + 1, m_PathEntryList.end());

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

    void ContentBrowserPanel::PruneMissingNodes(uint32_t nodeIndex, const std::filesystem::path& basePath)
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
				AssetHandle handle = Project::GetInstance()->GetAssetManager().GetAssetHandle(filepath);
				if (handle != AssetHandle(0))
				{
					ImGui::SetDragDropPayload("content_browser_item", &handle, sizeof(AssetHandle));
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

            AssetType assetType = GetAssetTypeFromExtension(filepath.extension().string());

            PendingFileLoading pf = { PendingFileLoading::ImportAssets, AssetMetaData(filepath, assetType), userData };
            cb->m_PendingAssetLoading.push(pf);
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
            return it->second.thumbnail;
        }

        // Create placeholder entry to prevent duplicate jobs
        FileThumbnail placeholder;
        placeholder.thumbnail = nullptr;
        placeholder.timestamp = 0;
        m_Thumbnails[filepath] = placeholder;

        // Capture by value to avoid dangling references
        std::filesystem::path capturedPath = filepath;
        int thumbnailSize = m_ThumbnailSize;

        Project::GetInstance()->GetAssetManager().SubmitJob([this, capturedPath, thumbnailSize]()
        {
            TextureCreateInfo createInfo;
            createInfo.format = nvrhi::Format::RGBA8_UNORM;
            createInfo.keepInitialState = true;
            createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
            createInfo.width = thumbnailSize;
            createInfo.height = thumbnailSize;

            // Load texture data on worker thread (no command list yet)
            Ref<Texture> loadedTexture = Texture::Create(capturedPath.string().c_str(), createInfo, nullptr);

            // Submit to main thread to create command list and finalize GPU upload
            Application::SubmitToMainThread([this, capturedPath, loadedTexture]() mutable
            {
                if (loadedTexture)
                {
                    nvrhi::IDevice *device = Application::GetGraphicsDevice();
                    nvrhi::CommandListHandle cmd = device->createCommandList();
                    cmd->open();
                    
                    loadedTexture->SetData(cmd, 4);
                    
                    cmd->close();
                    Application::SubmitWorkerCommandList(cmd);

                    FileThumbnail ft;
                    ft.thumbnail = loadedTexture;
                    
                    if (std::filesystem::exists(capturedPath))
                    {
                        ft.timestamp = std::filesystem::last_write_time(capturedPath).time_since_epoch().count();
                    }
                    else
                    {
                        ft.timestamp = 0;
                    }
                    
                    m_Thumbnails[capturedPath] = ft;
                }
                else
                {
                    // Remove placeholder if loading failed
                    m_Thumbnails.erase(capturedPath);
                }

                return true;
            });
        });

        return nullptr;
    }

    void ContentBrowserPanel::ClearThumbnails()
    {
        m_Thumbnails.clear();
    }
}
