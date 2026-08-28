// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PROJECT_HPP
#define IGN_PROJECT_HPP

#include "ignite/core/base.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/material_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/physics/2d/physics_2d.hpp"
#include "ignite/physics/3d/physics_3d.hpp"

#include <atomic>
#include <string>
#include <mutex>

#include "FileWatch.hpp"

namespace ignite
{
    class Scene;
    class ScriptEngine;

    enum class ProjectConfiguration
    {
        Debug = 0,
        Release,
        Shipping
    };

    struct ProjectInfo
    {
        std::string name;
        AssetHandle defaultSceneHandle = AssetHandle(0);

        std::filesystem::path filepath; // the actual project file (.ixproj)
        std::filesystem::path rootDirectory; // project directory
        std::filesystem::path assetDirectory = "Assets";
        std::filesystem::path scriptsDirectory = "Scripts";
        std::filesystem::path assetRegistryFilepath = "AssetRegistry.ixreg";

		physics::Physics3DType physics3DType = physics::Physics3DType::Jolt;
		physics::Physics3DSettings physicsSettings;
        ProjectConfiguration configuration = ProjectConfiguration::Debug;
    };

    using ProjectCallbackFn = std::function<void(bool)>;

    class IGN_API Project : public Asset
    {
    public:
        Project() = default;
        Project(const ProjectInfo &info);
        ~Project() override;

        void InitScriptEngine();

        std::filesystem::path GetProjectFilepath(const std::filesystem::path &filepath) const;
        std::filesystem::path GetProjectRelativeFilepath(const std::filesystem::path &filepath) const;


        void SetActiveScene(const Ref<Scene> &scene);
        void SetDefaultScene(AssetHandle handle);
        void BuildSolution(bool forceRebuild = false) const;

        void CreateCSharpScript(const std::filesystem::path &filepath);
        void CreateScriptableObject(const std::string &className, const std::string &fileName, const std::filesystem::path &targetDirectory);
        void RegenerateCSharpProject() const;

        std::vector<std::pair<AssetHandle, AssetMetaData>> ValidateAssetRegistry();

        std::filesystem::path GetFilepath() const { return m_Info.rootDirectory / m_Info.filepath; }

        const std::filesystem::path &GetDirectory() const { return m_Info.rootDirectory; }

        std::filesystem::path GetSolutionFilepath() const { return m_Info.rootDirectory / std::string(m_Info.name + ".slnx"); }

        std::filesystem::path GetAssetDirectory() const { return m_Info.rootDirectory / m_Info.assetDirectory; }

        std::filesystem::path GetScriptsDirectory() const { return m_Info.rootDirectory / m_Info.scriptsDirectory; }

        std::filesystem::path GetScriptBinDirectory() const { return m_Info.rootDirectory / "Bin"; }

        std::filesystem::path GetCacheDirectory() const { return m_Info.rootDirectory / ".cache"; }

        std::filesystem::path GetScriptModulePath() const
        {
            std::string configDir = "Debug";
            if (m_Info.configuration == ProjectConfiguration::Release) configDir = "Release";
            else if (m_Info.configuration == ProjectConfiguration::Shipping) configDir = "Shipping";
            return m_Info.rootDirectory / "Bin" / configDir / (m_Info.name + ".dll");
        }

        const std::string GetAssetDisplayName(AssetHandle handle) const;

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Project> Deserialize(const std::filesystem::path &filepath);

        MaterialManager &GetMaterialManager() { return m_MaterialManager; }

        ScriptEngine *GetScriptEngine() { return m_ScriptEngine; }
        ProjectInfo &GetInfo() { return m_Info; }

        WeakRef<Scene> GetActiveSceneWeak() const { return m_ActiveScene; }
		Ref<Scene> LockActiveScene() const { return m_ActiveScene; }

        physics::Physics3D *GetPhysics3D() { return m_Physics3D.get(); }
		physics::Physics2D *GetPhysics2D() { return m_Physics2D.get(); }

        ProjectConfiguration GetConfiguration() const { return m_Info.configuration; }

        static Ref<Project> Create(const ProjectInfo &info);

        static AssetType GetStaticType() { return AssetType::Project; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        bool IsCoreDependenciesUpToDate();

    private:
        void CopyCoreDependencies();
        void OnCoreDependencyChanged(const std::string &path, const filewatch::Event eventType);
        void StartCoreDependencyWatchers();

        void CreateDirectories() const;
        void GenerateProject();

        Ref<Scene> m_ActiveScene;
        ProjectInfo m_Info;

        MaterialManager m_MaterialManager;
		ScriptEngine *m_ScriptEngine = nullptr;

        Scope<physics::Physics2D> m_Physics2D;
        Scope<physics::Physics3D> m_Physics3D;

        std::map<std::string, bool> m_CoreDependencies;
        std::map<std::string, bool> m_CoreDependenciesPending;
        std::vector<Scope<filewatch::FileWatch<std::string>>> m_CoreDependencyWatchers;
        std::mutex m_CoreDependencyMutex;
    };
}

#endif
