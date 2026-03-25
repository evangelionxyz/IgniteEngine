//Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

#pragma once

#include "ignite/graphics/objects/material.hpp"

#include "ipanel.hpp"

#include <string>
#include <functional>

namespace ignite
{
    class MaterialsPanel : public IPanel
    {
    public:
        MaterialsPanel();
        ~MaterialsPanel();

        virtual void OnEvent(Event &event) override;

        void OnImGuiRender();

        bool IsOpen() const { return m_IsOpen; }
        void SetOpen(bool open) { m_IsOpen = open; }

        // Callback for when a material is selected
        void SetMaterialSelectionCallback(const std::function<void(Ref<Material>)> &callback) 
        { 
            m_OnMaterialSelected = callback; 
        }

        // Callback for when a material should be edited
        void SetMaterialEditCallback(const std::function<void(Ref<Material>)> &callback) 
        { 
            m_OnMaterialEdit = callback; 
        }

        void RefreshMaterialsList();

    private:
        void RenderMaterialItem(const std::string& materialName, Ref<Material> material);
        void RenderContextMenu(const std::string& materialName, Ref<Material> material);
        void CreateNewMaterial();
        void DuplicateMaterial(const std::string& materialName);
        void DeleteMaterial(const std::string& materialName);

        // Show confirmation dialog for destructive actions
        void ShowDeleteConfirmationModal();

    private:
        bool m_IsOpen = true;
        
        // Callbacks
        std::function<void(Ref<Material>)> m_OnMaterialSelected;
        std::function<void(Ref<Material>)> m_OnMaterialEdit;

        // For filtering/searching
        char m_SearchBuffer[256] = "";
        std::string m_SearchString;

        // For deletion confirmation
        bool m_ShowDeleteModal = false;
        std::string m_MaterialToDelete;
        
        // For creating new materials
        bool m_ShowCreateMaterialModal = false;
        char m_NewMaterialNameBuffer[256] = "NewMaterial";

        // Selected material for highlighting
        std::string m_SelectedMaterialName;
    };
}
