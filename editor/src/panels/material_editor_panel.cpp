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

#include "material_editor_panel.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/serializer/serializer.hpp"

#include "editor_layer.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace ignite
{
    MaterialEditorPanel::MaterialEditorPanel()
    {
    }

    MaterialEditorPanel::~MaterialEditorPanel()
    {
    }

    void MaterialEditorPanel::OnImGuiRender()
    {
        if (!m_IsOpen)
            return;

        if (ImGui::Begin("Material Editor", &m_IsOpen))
        {
            if (!m_SelectedMaterial)
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No material selected");
                ImGui::Separator();
                
                if (ImGui::Button("Create New Material"))
                {
                    if (Project::GetInstance())
                    {
                        auto& materialManager = Project::GetInstance()->GetMaterialManager();
                        m_SelectedMaterial = materialManager.CreateUniqueMaterial("NewMaterial");
                        m_MaterialName = m_SelectedMaterial->name;
                    }
                }
            }
            else
            {
                RenderMaterialProperties();
            }
        }
        ImGui::End();
    }

    void MaterialEditorPanel::SetSelectedMaterial(Ref<Material> material)
    {
        m_SelectedMaterial = material;
        if (material)
        {
            m_MaterialName = material->name;
        }
    }

    void MaterialEditorPanel::SetSelectedMaterial(const std::string &materialName)
    {
        if (Project::GetInstance())
        {
            auto& materialManager = Project::GetInstance()->GetMaterialManager();
            m_SelectedMaterial = materialManager.GetMaterial(materialName);
            if (m_SelectedMaterial)
            {
                m_MaterialName = materialName;
            }
        }
    }

    void MaterialEditorPanel::RenderMaterialProperties()
    {
        if (!m_SelectedMaterial)
            return;

        // Material name
        char nameBuffer[256];
        strcpy_s(nameBuffer, sizeof(nameBuffer), m_MaterialName.c_str());
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            m_MaterialName = nameBuffer;
            m_SelectedMaterial->name = m_MaterialName;
        }

        ImGui::Separator();

        // Material type and blend mode
        RenderMaterialTypeCombo();
        RenderBlendModeCombo();

        ImGui::Separator();

        // Basic properties
        if (ImGui::CollapsingHeader("Basic Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderColorProperty("Base Color", m_SelectedMaterial->params.baseColor);
            RenderFloatProperty("Metallic Factor", m_SelectedMaterial->params.metallicFactor);
            RenderFloatProperty("Roughness Factor", m_SelectedMaterial->params.roughnessFactor);
            RenderFloatProperty("Specular Factor", m_SelectedMaterial->params.specularFactor);
            RenderFloatProperty("Emissive Factor", m_SelectedMaterial->params.emissiveFactor);
        }

        // Texture slots
        if (ImGui::CollapsingHeader("Textures"))
        {
            RenderTextureSlot("Base Color", MaterialTextureType::BaseColor);
            RenderTextureSlot("Normal Map", MaterialTextureType::Normals);
            RenderTextureSlot("Metallic/Roughness", MaterialTextureType::Metalness);
            RenderTextureSlot("Emissive", MaterialTextureType::Emissive);
            RenderTextureSlot("Occlusion", MaterialTextureType::Occlusion);
            
            if (m_ShowAdvancedProperties)
            {
                RenderTextureSlot("Specular", MaterialTextureType::Specular);
                RenderTextureSlot("Height", MaterialTextureType::Height);
                RenderTextureSlot("Opacity", MaterialTextureType::Opacity);
            }
        }

        // Advanced properties toggle
        ImGui::Checkbox("Show Advanced Properties", &m_ShowAdvancedProperties);

        ImGui::Separator();

        // Actions
        if (ImGui::Button("Make Unique"))
        {
            if (Project::GetInstance())
            {
                auto& materialManager = Project::GetInstance()->GetMaterialManager();
                std::string uniqueName = m_SelectedMaterial->name + "_Copy";
                auto uniqueMaterial = materialManager.CloneMaterial(m_SelectedMaterial->name, uniqueName);
                if (uniqueMaterial)
                {
                    SetSelectedMaterial(uniqueMaterial);
                }
            }
        }

        ImGui::SameLine();
        
        if (ImGui::Button("Save Material"))
        {
            // TODO: Implement material saving
            // For now, just mark as dirty so it gets saved with the project
            m_SelectedMaterial->SetDirtyFlag(true);

            std::filesystem::path filepath = FileDialogs::SaveFile("Material (*.ixmat)\0(*.ixmat)");
            if (!filepath.empty())
            {
                BinarySerializer::SerializeMaterial(m_SelectedMaterial, filepath);
                Project::GetInstance()->GetAssetManager().ImportAsset(filepath);

                ProjectSerializer serializer(Project::GetInstance());
                serializer.Serialize(Project::GetInstance()->GetFilepath());
            }
        }
    }

    void MaterialEditorPanel::RenderTextureSlot(const char* label, MaterialTextureType textureType)
    {
        ImGui::Text("%s", label);
        ImGui::SameLine();

        auto& textures = m_SelectedMaterial->textures;
        bool hasTexture = textures.contains(textureType) && textures[textureType];

        if (hasTexture)
        {
            ImGui::Text("Assigned");
            ImGui::SameLine();
            if (ImGui::Button(("Remove##" + std::string(label)).c_str()))
            {
                textures.erase(textureType);
                m_SelectedMaterial->UpdateBindingSet();
            }
        }
        else
        {
            if (ImGui::Button(("Browse##" + std::string(label)).c_str()))
            {
                // TODO: Open file dialog to select texture
                // For now, this is a placeholder
            }
        }

        // Drag and drop support
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                if (payload->DataSize == sizeof(AssetHandle))
                {
                    AssetHandle* handle = static_cast<AssetHandle*>(payload->Data);
                    if (handle && *handle != AssetHandle(0))
                    {
                        if (Project::GetInstance())
                        {
                            auto& assetManager = Project::GetInstance()->GetAssetManager();
                            AssetMetaData metadata = assetManager.GetMetaData(*handle);
                            if (metadata.type == AssetType::Texture)
                            {
                                Ref<Texture> texture = Project::GetInstance()->GetAsset<Texture>(*handle);
                                if (texture)
                                {
                                    m_SelectedMaterial->UpdateTexture(texture, textureType);
                                }
                            }
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void MaterialEditorPanel::RenderColorProperty(const char* label, glm::vec4& color)
    {
        ImGui::ColorEdit4(label, &color.r);
    }

    void MaterialEditorPanel::RenderFloatProperty(const char* label, float& value, float min, float max)
    {
        ImGui::SliderFloat(label, &value, min, max);
    }

    void MaterialEditorPanel::RenderMaterialTypeCombo()
    {
        const char* materialTypes[] = { "PBR", "Legacy" };
        const char* currentType = materialTypes[static_cast<int>(m_SelectedMaterial->type)];

        if (ImGui::BeginCombo("Material Type", currentType))
        {
            for (int i = 0; i < IM_ARRAYSIZE(materialTypes); i++)
            {
                bool isSelected = (static_cast<int>(m_SelectedMaterial->type) == i);
                if (ImGui::Selectable(materialTypes[i], isSelected))
                {
                    m_SelectedMaterial->type = static_cast<MaterialType>(i);
                }
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void MaterialEditorPanel::RenderBlendModeCombo()
    {
        const char* blendModes[] = { "Opaque", "Transparent", "Additive" };
        const char* currentBlendMode = blendModes[static_cast<int>(m_SelectedMaterial->blendMode)];

        if (ImGui::BeginCombo("Blend Mode", currentBlendMode))
        {
            for (int i = 0; i < IM_ARRAYSIZE(blendModes); i++)
            {
                bool isSelected = (static_cast<int>(m_SelectedMaterial->blendMode) == i);
                if (ImGui::Selectable(blendModes[i], isSelected))
                {
                    m_SelectedMaterial->blendMode = static_cast<MaterialBlendMode>(i);
                }
                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
}
