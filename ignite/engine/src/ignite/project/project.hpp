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
#include "ignite/core/path.hpp"
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

        ignite::Path filepath; // the actual project file (.ixproj)
        ignite::Path rootDirectory; // project directory
        ignite::Path assetDirectory = "Assets";
        ignite::Path scriptsDirectory = "Scripts";
        ignite::Path assetRegistryFilepath = "AssetRegistry.ixreg";

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

        ignite::Path GetProjectFilepath(const ignite::Path &filepath) const;
        ignite::Path GetProjectRelativeFilepath(const ignite::Path &filepath) const;

        void CopyCoreDependencies();

        void SetActiveScene(const Ref<Scene> &scene);
        void SetDefaultScene(AssetHandle handle);
        void BuildSolution(bool forceRebuild = false) const;

        void CreateCSharpScript(const ignite::Path &filepath);
        void CreateScriptableObject(const std::string &className, const std::string &fileName, const ignite::Path &targetDirectory);
        void RegenerateCSharpProject() const;

        std::vector<std::pair<AssetHandle, AssetMetaData>> ValidateAssetRegistry();

        ignite::Path GetFilepath() const { return m_Info.rootDirectory / m_Info.filepath; }

        const ignite::Path &GetDirectory() const { return m_Info.rootDirectory; }

        ignite::Path GetSolutionFilepath() const { return m_Info.rootDirectory / std::string(m_Info.name + ".slnx"); }

        ignite::Path GetAssetDirectory() const { return m_Info.rootDirectory / m_Info.assetDirectory; }

        ignite::Path GetScriptsDirectory() const { return m_Info.rootDirectory / m_Info.scriptsDirectory; }

        ignite::Path GetScriptBinDirectory() const { return m_Info.rootDirectory / "Bin"; }

        ignite::Path GetCacheDirectory() const { return m_Info.rootDirectory / ".cache"; }

        ignite::Path GetScriptModulePath() const
        {
            std::string configDir = "Debug";
            if (m_Info.configuration == ProjectConfiguration::Release) configDir = "Release";
            else if (m_Info.configuration == ProjectConfiguration::Shipping) configDir = "Shipping";
            return m_Info.rootDirectory / "Bin" / configDir / (m_Info.name + ".dll");
        }

        const std::string GetAssetDisplayName(AssetHandle handle) const;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<Project> Deserialize(const ignite::Path &filepath);

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

        void StartCoreDependencyWatchers();
        void OnCoreDependencyChanged(const std::string &path, const filewatch::Event eventType);

    private:
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
