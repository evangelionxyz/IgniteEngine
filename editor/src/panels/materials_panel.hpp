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

#pragma once

#include "ignite/graphics/objects/material.hpp"
#include <string>
#include <functional>

namespace ignite
{
    class MaterialsPanel
    {
    public:
        MaterialsPanel();
        ~MaterialsPanel();

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
