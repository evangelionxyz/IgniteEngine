// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef MATERIAL_MANAGER_HPP
#define MATERIAL_MANAGER_HPP

#include "ignite/core/types.hpp"
#include "asset.hpp"

#include <string>
#include <map>
#include "ignite/core/path.hpp"

namespace ignite
{
    class Material;
    class Project;

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
        bool SaveMaterial(const std::string &name, const ignite::Path &filepath);
        Ref<Material> LoadMaterial(const ignite::Path &filepath);
        
        void Clear();
        
    private:
        std::string GenerateUniqueName(const std::string &baseName);
        
        std::map<std::string, Ref<Material>> m_Materials;
        std::map<AssetHandle, std::string> m_AssetToMaterialMap; // Maps asset handles to material names
    };
}

#endif