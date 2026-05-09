// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef PROJECT_HPP
#define PROJECT_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/material_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"

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
        std::filesystem::path rootDirectory; // project directory
        std::filesystem::path scriptModuleFilepath; // .dll Script file
        std::filesystem::path assetDirectory = "Assets";
        std::filesystem::path scriptsDirectory = "Scripts";
        std::filesystem::path assetRegistryFilepath = "AssetRegistry.ixreg";
    };

    class Project : public Asset
    {
    public:
        Project() = default;
        Project(const ProjectInfo &info);

        ~Project() override;
        
        std::filesystem::path GetProjectFilepath(const std::filesystem::path &filepath) const;
        std::filesystem::path GetProjectRelativeFilepath(const std::filesystem::path &filepath) const;
        
        void SetActiveScene(const Ref<Scene> &scene);
        void SetDefaultScene(AssetHandle handle);
        bool BuildSolution();
        
        void CreateCSharpScript(const std::filesystem::path &filepath);
        void CreateScriptableObject(const std::string &className, const std::string &fileName, const std::filesystem::path &targetDirectory);
        void RegenerateCSharpProject();

        std::vector<std::pair<AssetHandle, AssetMetaData>> ValidateAssetRegistry();

        std::filesystem::path GetFilepath() const
        {
            return m_Info.rootDirectory / m_Info.filepath;
        }

        const std::filesystem::path &GetDirectory() const
        {
            return m_Info.rootDirectory;
        }

        std::filesystem::path GetSolutionFilepath() const
        {
            return m_Info.rootDirectory / std::string(m_Info.name + ".slnx");
        }

        std::filesystem::path GetAssetDirectory() const
        {
            return m_Info.rootDirectory / m_Info.assetDirectory;
        }

        std::filesystem::path GetScriptsDirectory() const
        {
            return m_Info.rootDirectory / m_Info.scriptsDirectory;
        }

        std::filesystem::path GetScriptBinDirectory() const
        {
            return m_Info.rootDirectory / "Bin";
        }

        std::filesystem::path GetScriptModulePath() const
        {
            return m_Info.rootDirectory / m_Info.scriptModuleFilepath;
        }

        template<typename T>
        Ref<T> GetAsset(AssetHandle handle)
        {
            return m_AssetManager->GetAsset<T>(handle);
        }

        template<typename T>
        Ref<T> GetAssetImmediate(AssetHandle handle)
        {
            return m_AssetManager->GetAssetImmediate<T>(handle);
        }

        const std::string GetAssetDisplayName(AssetHandle handle) const;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Project> Deserialize(const std::filesystem::path &filepath);

        AssetManager *GetAssetManager() { return m_AssetManager; }
        MaterialManager &GetMaterialManager() { return m_MaterialManager; }

        ScriptEngine *GetScriptEngine() { return m_ScriptEngine; }
        ProjectInfo &GetInfo() { return m_Info; }
        Ref<Scene> GetActiveScene() const { return m_ActiveScene; }

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

#endif
