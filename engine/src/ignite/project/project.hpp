// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef PROJECT_HPP
#define PROJECT_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/material_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include <string>
#include "ignite/core/path.hpp"

namespace ignite
{
    class Scene;
    class ScriptEngine;
    
    struct ProjectInfo
    {
        std::string name;
        AssetHandle defaultSceneHandle = AssetHandle(0);

        ignite::Path filepath; // the actual project file (.ixproj)
        ignite::Path rootDirectory; // project directory
        ignite::Path scriptModuleFilepath; // .dll Script file
        ignite::Path assetDirectory = "Assets";
        ignite::Path scriptsDirectory = "Scripts";
        ignite::Path assetRegistryFilepath = "AssetRegistry.ixreg";
    };

    class Project : public Asset
    {
    public:
        Project() = default;
        Project(const ProjectInfo &info);

        ~Project() override;
        
        ignite::Path GetProjectFilepath(const ignite::Path &filepath) const;
        ignite::Path GetProjectRelativeFilepath(const ignite::Path &filepath) const;
        
        void SetActiveScene(const Ref<Scene> &scene);
        void SetDefaultScene(AssetHandle handle);
        bool BuildSolution();
        
        void CreateCSharpScript(const ignite::Path &filepath);
        void CreateScriptableObject(const std::string &className, const std::string &fileName, const ignite::Path &targetDirectory);
        void RegenerateCSharpProject();

        std::vector<std::pair<AssetHandle, AssetMetaData>> ValidateAssetRegistry();

        ignite::Path GetFilepath() const
        {
            return m_Info.rootDirectory / m_Info.filepath;
        }

        const ignite::Path &GetDirectory() const
        {
            return m_Info.rootDirectory;
        }

        ignite::Path GetSolutionFilepath() const
        {
            return m_Info.rootDirectory / std::string(m_Info.name + ".slnx");
        }

        ignite::Path GetAssetDirectory() const
        {
            return m_Info.rootDirectory / m_Info.assetDirectory;
        }

        ignite::Path GetScriptsDirectory() const
        {
            return m_Info.rootDirectory / m_Info.scriptsDirectory;
        }

        ignite::Path GetScriptBinDirectory() const
        {
            return m_Info.rootDirectory / "Bin";
        }

        ignite::Path GetScriptModulePath() const
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

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<Project> Deserialize(const ignite::Path &filepath);

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
