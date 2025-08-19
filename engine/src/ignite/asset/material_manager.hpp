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
#include "asset.hpp"

#include <string>
#include <map>
#include <filesystem>

namespace ignite
{
    class Material;

    class MaterialManager
    {
    public:
        MaterialManager();
        ~MaterialManager();

        Ref<Material> CreateMaterial(const std::string &name);
        Ref<Material> CreateUniqueMaterial(const std::string &baseName);
        Ref<Material> CloneMaterial(const std::string &originalName, const std::string &newName);
        
        void AddMaterial(const std::string &name, Ref<Material> material);
        void RemoveMaterial(const std::string &name);
        bool HasMaterial(const std::string &name) const;
        
        Ref<Material> GetMaterial(const std::string &name);
        const std::map<std::string, Ref<Material>> &GetMaterials() const { return m_Materials; }
        
        // Asset integration
        void RegisterMaterialAsset(AssetHandle handle, Ref<Material> material);
        void UnregisterMaterialAsset(AssetHandle handle);
        Ref<Material> GetMaterialByHandle(AssetHandle handle);
        
        // Save/Load materials
        bool SaveMaterial(const std::string &name, const std::filesystem::path &filepath);
        Ref<Material> LoadMaterial(const std::filesystem::path &filepath);
        
        void Clear();
        
    private:
        std::string GenerateUniqueName(const std::string &baseName);
        
        std::map<std::string, Ref<Material>> m_Materials;
        std::map<AssetHandle, std::string> m_AssetToMaterialMap; // Maps asset handles to material names
    };
}