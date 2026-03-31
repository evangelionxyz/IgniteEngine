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

#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/material_manager.hpp"

#include <string>
#include <filesystem>

namespace ignite
{
    class Scene;
    class ScriptEngine;
    
    struct ProjectInfo
    {
        std::string name;
        AssetHandle defaultSceneHandle = AssetHandle(0);

        std::filesystem::path filepath; // the actual project file (.ixproj)
        std::filesystem::path scriptModuleFilepath;
        std::filesystem::path assetDirectory = "Assets";
        std::filesystem::path scriptsDirectory = "Scripts";
        std::filesystem::path assetRegistryFilepath = "AssetRegistry.ixreg";
        std::filesystem::path premakeFilepath = "premake5.lua";
    };

    class Project : public Asset
    {
    public:
        Project() = default;
        Project(const ProjectInfo &info);

        ~Project() override;
        
        std::filesystem::path GetAssetRelativeFilepath(const std::filesystem::path &filepath) const;
        std::filesystem::path GetAssetFilepath(const std::filesystem::path &filepath) const;
        std::filesystem::path GetRelativeFilepath(const std::filesystem::path &filepath) const;
        
        void SetActiveScene(const Ref<Scene> &scene);
        void SetDefaultScene(AssetHandle handle);
        bool BuildSolution();

        std::vector<std::pair<AssetHandle, AssetMetaData>> ValidateAssetRegistry();

        std::filesystem::path GetFilepath() const
        {
            return GetDirectory() / m_Info.filepath;
        }

        std::filesystem::path GetDirectory() const
        {
            return m_Info.filepath.parent_path();
        }

        std::filesystem::path GetSolutionFilepath() const
        {
            return GetDirectory() / std::string(m_Info.name + ".slnx");
        }

        std::filesystem::path GetAssetDirectory() const
        {
            return GetDirectory() / m_Info.assetDirectory;
        }

        std::filesystem::path GetScriptsDirectory() const
        {
            return GetDirectory() / m_Info.scriptsDirectory;
        }

        std::filesystem::path GetScriptBinDirectory() const
        {
            return GetDirectory() / "Bin";
        }

        std::filesystem::path GetScriptModulePath() const
        {
            return GetDirectory() / m_Info.scriptModuleFilepath;
        }

        template<typename T>
        Ref<T> GetAsset(AssetHandle handle, AssetType requestAssetType = AssetType::Auto)
        {
            Ref<Asset> asset = m_AssetManager->GetAsset(handle, requestAssetType);
            if (!asset)
            {
                return nullptr;
            }
            return std::static_pointer_cast<T>(asset);
        }

        template<typename T>
        Ref<T> GetAssetImmediate(AssetHandle handle, AssetType requestAssetType = AssetType::Auto)
        {
            Ref<Asset> asset = m_AssetManager->GetAssetImmediate(handle, requestAssetType);
            if (!asset)
            {
                return nullptr;
            }
            return std::static_pointer_cast<T>(asset);
        }

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Project> Deserialize(const std::filesystem::path &filepath);

        AssetManager &GetAssetManager() { return *m_AssetManager; }
        MaterialManager &GetMaterialManager() { return m_MaterialManager; }

        ScriptEngine *GetScriptEngine() { return m_ScriptEngine; }
        ProjectInfo &GetInfo() { return m_Info; }
        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }

        static Project *GetInstance();
        static Ref<Project> Create(const ProjectInfo &info);

        static AssetType GetStaticType() { return AssetType::Project; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void CreateDirectories();
        void CopyDependencies();
        void GenerateProject();

        Ref<Scene> m_ActiveScene; // current active scene in editor
        ProjectInfo m_Info;

        MaterialManager m_MaterialManager;
        AssetManager *m_AssetManager = nullptr;
        ScriptEngine *m_ScriptEngine = nullptr;
    };

}
