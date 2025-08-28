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

#include "materials_panel.hpp"
#include "ignite/serializer/binary_serializer.hpp"

#include "ignite/project/project.hpp"
#include "ignite/asset/material_manager.hpp"
#include "ignite/core/platform_utils.hpp"

#include "editor_layer.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

namespace ignite
{
    MaterialsPanel::MaterialsPanel()
    {
    }

    MaterialsPanel::~MaterialsPanel()
    {
    }

    void MaterialsPanel::OnImGuiRender()
    {
        if (!m_IsOpen)
            return;

        if (ImGui::Begin("Materials", &m_IsOpen))
        {
            if (!Project::GetInstance())
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No project loaded");
                ImGui::End();
                return;
            }

            auto& materialManager = Project::GetInstance()->GetMaterialManager();

            // Toolbar
            if (ImGui::Button("New Material"))
            {
                m_ShowCreateMaterialModal = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Refresh"))
            {
                RefreshMaterialsList();
            }

            // Search bar
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputTextWithHint("##Search", "Search materials...", m_SearchBuffer, sizeof(m_SearchBuffer)))
            {
                m_SearchString = m_SearchBuffer;
                std::transform(m_SearchString.begin(), m_SearchString.end(), m_SearchString.begin(), ::tolower);
            }

            ImGui::Separator();

            // Materials list
            if (ImGui::BeginChild("MaterialsList"))
            {
                const auto& materials = materialManager.GetMaterials();
                
                if (materials.empty())
                {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No materials in project");
                }
                else
                {
                    for (const auto& [name, material] : materials)
                    {
                        if (!m_SearchString.empty())
                        {
                            std::string lowerName = name;
                            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                            if (lowerName.find(m_SearchString) == std::string::npos)
                                continue;
                        }

                        RenderMaterialItem(name, material);
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // Modals
        if (m_ShowCreateMaterialModal)
        {
            ImGui::OpenPopup("Create Material");
            m_ShowCreateMaterialModal = false;
        }

        if (m_ShowDeleteModal)
        {
            ShowDeleteConfirmationModal();
        }

        // Create Material Modal
        if (ImGui::BeginPopupModal("Create Material", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Material Name:");
            ImGui::InputText("##MaterialName", m_NewMaterialNameBuffer, sizeof(m_NewMaterialNameBuffer));

            ImGui::Separator();

            if (ImGui::Button("Create"))
            {
                CreateNewMaterial();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void MaterialsPanel::RefreshMaterialsList()
    {
        // The material manager already maintains the list, so we just need to trigger a refresh
        // This could be useful for reloading from disk or updating after external changes
    }

    void MaterialsPanel::RenderMaterialItem(const std::string& materialName, Ref<Material> material)
    {
        bool isSelected = (m_SelectedMaterialName == materialName);
        
        if (ImGui::Selectable(materialName.c_str(), isSelected))
        {
            m_SelectedMaterialName = materialName;
            if (m_OnMaterialSelected)
            {
                m_OnMaterialSelected(material);
            }
        }

        // Double-click to edit
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (m_OnMaterialEdit)
            {
                m_OnMaterialEdit(material);
            }
        }

        // Context menu
        if (ImGui::BeginPopupContextItem())
        {
            RenderContextMenu(materialName, material);
            ImGui::EndPopup();
        }

        // Drag source for material assignment
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("MATERIAL", materialName.c_str(), materialName.size() + 1);
            ImGui::Text("Material: %s", materialName.c_str());
            ImGui::EndDragDropSource();
        }
    }

    void MaterialsPanel::RenderContextMenu(const std::string& materialName, Ref<Material> material)
    {
        if (ImGui::MenuItem("Edit"))
        {
            if (m_OnMaterialEdit)
            {
                m_OnMaterialEdit(material);
            }
        }

        if (ImGui::MenuItem("Duplicate"))
        {
            DuplicateMaterial(materialName);
        }

        if (ImGui::MenuItem("Make Unique"))
        {
            if (Project::GetInstance())
            {
                auto& materialManager = Project::GetInstance()->GetMaterialManager();
                std::string uniqueName = materialName + "_Copy";
                materialManager.CloneMaterial(materialName, uniqueName);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rename"))
        {
            // TODO: Implement inline renaming
        }

        if (ImGui::MenuItem("Delete", nullptr, false, materialName != "Default"))
        {
            m_MaterialToDelete = materialName;
            m_ShowDeleteModal = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Show in Asset Manager"))
        {
            // TODO: Navigate to material in asset manager
        }
    }

    void MaterialsPanel::CreateNewMaterial()
    {
        if (Project::GetInstance())
        {
            auto& materialManager = Project::GetInstance()->GetMaterialManager();

            std::filesystem::path folder = FileDialogs::SelectFolder();
            if (!folder.empty())
            {
                std::string materialName = m_NewMaterialNameBuffer;

                auto newMaterial = materialManager.CreateUniqueMaterial(materialName);
                if (newMaterial && m_OnMaterialEdit)
                {
                    m_OnMaterialEdit(newMaterial);
                }

                // Serialize
                if (newMaterial)
                {
                    std::filesystem::path materialSavePath = folder / (materialName + ".ixmat");
                    BinarySerializer::SerializeMaterial(newMaterial, materialSavePath);
                    Project::GetInstance()->GetAssetManager().ImportAsset(materialSavePath);
                }

                // Reset the name buffer
                strcpy_s(m_NewMaterialNameBuffer, sizeof(m_NewMaterialNameBuffer), "NewMaterial");
            }
        }
    }

    void MaterialsPanel::DuplicateMaterial(const std::string& materialName)
    {
        if (Project::GetInstance())
        {
            auto& materialManager = Project::GetInstance()->GetMaterialManager();
            std::string duplicateName = materialName + "_Copy";
            materialManager.CloneMaterial(materialName, duplicateName);
        }
    }

    void MaterialsPanel::DeleteMaterial(const std::string& materialName)
    {
        if (Project::GetInstance())
        {
            auto& materialManager = Project::GetInstance()->GetMaterialManager();
            materialManager.RemoveMaterial(materialName);
            
            // Clear selection if this was the selected material
            if (m_SelectedMaterialName == materialName)
            {
                m_SelectedMaterialName.clear();
            }
        }
    }

    void MaterialsPanel::ShowDeleteConfirmationModal()
    {
        if (ImGui::BeginPopupModal("Delete Material", &m_ShowDeleteModal, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Are you sure you want to delete material '%s'?", m_MaterialToDelete.c_str());
            ImGui::Text("This action cannot be undone.");

            ImGui::Separator();

            if (ImGui::Button("Delete"))
            {
                DeleteMaterial(m_MaterialToDelete);
                m_ShowDeleteModal = false;
                m_MaterialToDelete.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_ShowDeleteModal = false;
                m_MaterialToDelete.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
