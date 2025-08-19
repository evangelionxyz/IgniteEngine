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

#include "ignite/core/types.hpp"
#include "ignite/graphics/material.hpp"

#include <string>

namespace ignite
{
    class MaterialEditorPanel
    {
    public:
        MaterialEditorPanel();
        ~MaterialEditorPanel();

        void OnImGuiRender();
        void SetSelectedMaterial(Ref<Material> material);
        void SetSelectedMaterial(const std::string &materialName);
        
        bool IsOpen() const { return m_IsOpen; }
        void SetOpen(bool open) { m_IsOpen = open; }

    private:
        void RenderMaterialProperties();
        void RenderTextureSlot(const char* label, MaterialTextureType textureType);
        void RenderColorProperty(const char* label, glm::vec4& color);
        void RenderFloatProperty(const char* label, float& value, float min = 0.0f, float max = 1.0f);
        void RenderMaterialTypeCombo();
        void RenderBlendModeCombo();
        
        bool m_IsOpen = false;
        Ref<Material> m_SelectedMaterial;
        std::string m_MaterialName;
        
        // UI state
        bool m_ShowAdvancedProperties = false;
    };
}
