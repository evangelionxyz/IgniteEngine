// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "material_manager.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
    MaterialManager::MaterialManager()
    {
    }

    MaterialManager::~MaterialManager()
    {
    }

    Ref<Material> MaterialManager::CreateMaterial(const std::string &name)
    {
        if (HasMaterial(name))
        {
            LOG_WARN("[MaterialManager] Material '{}' already exists", name);
            return GetMaterial(name);
        }

        Ref<Material> mat = CreateRef<Material>();

        mat->name = name;
        m_Materials[name] = mat;
        return mat;
    }

    Ref<Material> MaterialManager::CreateUniqueMaterial(const std::string &baseName)
    {
        std::string uniqueName = GenerateUniqueName(baseName);
        return CreateMaterial(uniqueName);
    }

    Ref<Material> MaterialManager::CloneMaterial(const std::string &originalName, const std::string &newName)
    {
        auto originalMat = GetMaterial(originalName);
        if (!originalMat)
        {
            LOG_ERROR("[MaterialManager] Cannot clone material '{}' - not found", originalName);
            return nullptr;
        }

        if (HasMaterial(newName))
        {
            LOG_WARN("[MaterialManager] Material '{}' already exists, cannot clone", newName);
            return GetMaterial(newName);
        }

        Ref<Material> clonedMat = CreateRef<Material>(*originalMat);
        clonedMat->name = newName;
        // clonedMat->handle = AssetHandle(); // Generate new handle
        m_Materials[newName] = clonedMat;

        return clonedMat;
    }

    void MaterialManager::AddMaterial(const std::string &name, Ref<Material> material)
    {
        if (!material)
        {
            LOG_ERROR("[MaterialManager] Cannot add null material");
            return;
        }

        material->name = name;
        m_Materials[name] = material;
    }

    void MaterialManager::RemoveMaterial(const std::string &name)
    {
        if (auto it = m_Materials.find(name); it != m_Materials.end())
        {
            // Remove from asset mapping if exists
            for (auto assetIt = m_AssetToMaterialMap.begin(); assetIt != m_AssetToMaterialMap.end();)
            {
                if (assetIt->second == name)
                {
                    assetIt = m_AssetToMaterialMap.erase(assetIt);
                }
                else
                {
                    ++assetIt;
                }
            }

            m_Materials.erase(it);
            LOG_INFO("[MaterialManager] Removed material '{}'", name);
        }
    }

    bool MaterialManager::HasMaterial(const std::string &name) const
    {
        return m_Materials.contains(name);
    }

    Ref<Material> MaterialManager::GetMaterial(const std::string &name)
    {
        if (auto it = m_Materials.find(name); it != m_Materials.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void MaterialManager::RegisterMaterialAsset(AssetHandle handle, Ref<Material> material)
    {
        if (!material)
        {
            LOG_ERROR("[MaterialManager] Cannot register null material asset");
            return;
        }

        // material->handle = handle;
        m_AssetToMaterialMap[handle] = material->name;

        if (!HasMaterial(material->name))
        {
            m_Materials[material->name] = material;
        }
    }

    void MaterialManager::UnregisterMaterialAsset(AssetHandle handle)
    {
        if (auto it = m_AssetToMaterialMap.find(handle); it != m_AssetToMaterialMap.end())
        {
            m_AssetToMaterialMap.erase(it);
        }
    }

    Ref<Material> MaterialManager::GetMaterialByHandle(AssetHandle handle)
    {
        if (auto it = m_AssetToMaterialMap.find(handle); it != m_AssetToMaterialMap.end())
        {
            return GetMaterial(it->second);
        }
        return nullptr;
    }

    bool MaterialManager::SaveMaterial(const std::string &name, const std::filesystem::path &filepath)
    {
        auto material = GetMaterial(name);
        if (!material)
        {
            LOG_ERROR("[MaterialManager] Cannot save material '{}' - not found", name);
            return false;
        }

        // TODO: Implement material serialization
        LOG_INFO("[MaterialManager] Material serialization not yet implemented");
        return false;
    }

    Ref<Material> MaterialManager::LoadMaterial(const std::filesystem::path &filepath)
    {
        // TODO: Implement material deserialization
        LOG_INFO("[MaterialManager] Material deserialization not yet implemented");
        return nullptr;
    }

    void MaterialManager::Clear()
    {
        m_Materials.clear();
        m_AssetToMaterialMap.clear();
    }

    std::string MaterialManager::GenerateUniqueName(const std::string &baseName)
    {
        if (!HasMaterial(baseName))
        {
            return baseName;
        }

        int counter = 1;
        std::string uniqueName;
        do
        {
            uniqueName = std::format("{}_{}", baseName, counter);
            counter++;
        }
        while (HasMaterial(uniqueName));

        return uniqueName;
    }
}
