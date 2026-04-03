// Copyright (c) 2026 Evangelion Manuhutu

#include "project.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/core/platform_utils.hpp"

#include "ignite/serializer/serializer.hpp"

#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <fstream>
#include <format>

namespace ignite
{
    static std::string s_CSSharpScriptTemplate =
R"(using Ignite;
using System;

namespace {PROJECT_NAME};
public class Game : Entity
{
    public override void OnCreate()
    {
        // Initialize you object here
        Console.WriteLine("Hello From C#!");
    }

    public override void OnUpdate(float deltaTime)
    {
        // Update loop
    }
}
)";

    static std::string s_SlnxTemplate = 
R"(<Solution Description="Visual Studio slnx" Version="1.4">
  <Configurations>
    <BuildType Name="Release" />
  </Configurations>
  <Project Path="{PROJECT_NAME}.csproj" Id="{GENERATED_GUID}" />
</Solution>
)";

    static std::string s_CSProjTemplate =
R"(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Library</OutputType>
    <AppDesignerFolder>Properties</AppDesignerFolder>
    <TargetFramework>net9.0</TargetFramework>
    <Configurations>Release</Configurations>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
  </PropertyGroup>
  <PropertyGroup Condition=" '$(Configuration)|$(Platform)' == 'Release|AnyCPU' ">
    <PlatformTarget>AnyCPU</PlatformTarget>
    <DebugType>portable</DebugType>
    <DebugSymbols>true</DebugSymbols>
    <Optimize>true</Optimize>
    <OutputPath>Bin\</OutputPath>
    <IntermediateOutputPath>Bin\Objs\</IntermediateOutputPath>
    <DefineConstants></DefineConstants>
    <ErrorReport>prompt</ErrorReport>
    <WarningLevel>4</WarningLevel>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    <CopyLocalLockFileAssemblies>true</CopyLocalLockFileAssemblies>
    <EnableDynamicLoading>true</EnableDynamicLoading>
    <ImplicitUsing>enable</ImplicitUsing>
    <Nullable>enable</Nullable>
  </PropertyGroup>
  <ItemGroup Condition=" '$(Configuration)|$(Platform)' == 'Release|AnyCPU' ">
    <Reference Include="MochiSharp.Managed">
      <HintPath>Bin\MochiSharp.Managed.dll</HintPath>
    </Reference>
  </ItemGroup>
  <ItemGroup>
    <Compile Include="Scripts\Game.cs" />
  </ItemGroup>
  <ItemGroup>
    <Reference Include="IgniteScriptEngine">
      <HintPath>Bin\IgniteScriptEngine.dll</HintPath>
    </Reference>
  </ItemGroup>
</Project>
)";

    Project *project = nullptr;

    Project::Project(const ProjectInfo &info)
        : m_Info(info)
    {
        project = this;
        GenerateProject();

        m_AssetManager = new AssetManager(this);
        m_ScriptEngine = new ScriptEngine(this);
    }

    Project::~Project()
    {
        project = nullptr;

        delete m_ScriptEngine;
        delete m_AssetManager;
    }

    std::filesystem::path Project::GetAssetRelativeFilepath(const std::filesystem::path &filepath) const
    {
        auto basePath = m_Info.filepath.parent_path() / m_Info.assetDirectory;
        return std::filesystem::relative(filepath, basePath);
    }

    std::filesystem::path Project::GetAssetFilepath(const std::filesystem::path &filepath) const
    {
        return m_Info.filepath.parent_path() / m_Info.assetDirectory / filepath;
    }

    std::filesystem::path Project::GetRelativeFilepath(const std::filesystem::path &filepath) const
    {
        return std::filesystem::relative(filepath, m_Info.filepath.parent_path());
    }

    void Project::SetActiveScene(const Ref<Scene> &scene)
    {
        m_ActiveScene = scene;
    }

    void Project::SetDefaultScene(AssetHandle handle)
    {
        m_Info.defaultSceneHandle = handle;
    }

    std::vector<std::pair<AssetHandle, AssetMetaData>> Project::ValidateAssetRegistry()
    {
        std::vector<std::pair<AssetHandle, AssetMetaData>> invalidRegistry;
        AssetRegistry &assetRegistry = GetAssetManager().GetAssetAssetRegistry();

        for (auto it = assetRegistry.begin(); it != assetRegistry.end();)
        {
            const std::filesystem::path &filepath = GetAssetFilepath(it->second.filepath);
            if (!std::filesystem::exists(filepath))
            {
                invalidRegistry.emplace_back(it->first, it->second);
                it = assetRegistry.erase(it);
            }
            else
            {
                ++it;
            }
        }

        return invalidRegistry;
    }

	bool Project::Serialize(const std::filesystem::path &filepath)
	{
		Serializer projectSr(filepath);

		projectSr.BeginMap(); // START

		{
			projectSr.BeginMap("Project");

			projectSr.AddKeyValue("Version", ENGINE_VERSION);
			projectSr.AddKeyValue("Name", m_Info.name);
			projectSr.AddKeyValue("AssetPath", m_Info.assetDirectory.generic_string());
			projectSr.AddKeyValue("AssetRegistry", m_Info.assetRegistryFilepath.generic_string());
			projectSr.AddKeyValue("ScriptModule", m_Info.scriptModuleFilepath.generic_string());
			projectSr.AddKeyValue("DefaultSceneHandle", m_Info.defaultSceneHandle);

			projectSr.EndMap();
		}

		projectSr.EndMap(); // END
		projectSr.Serialize();
		// set dirty flags
		this->SetDirtyFlag(false);


		// Serialize asset manager
		auto &assetRegistry = m_AssetManager->GetAssetAssetRegistry();

		{
			const std::filesystem::path assetRegFilepath = filepath.parent_path() / m_Info.assetRegistryFilepath;
			Serializer assetSr(assetRegFilepath);

			assetSr.BeginMap(); // Start

			assetSr.BeginMap("AssetRegistry");

			assetSr.BeginSequence("Assets"); // Asset sequence
			for (auto &[handle, metadata] : assetRegistry)
			{
				assetSr.BeginMap(); // Begin Metadata

				assetSr.AddKeyValue("Handle", static_cast<uint64_t>(handle));
				assetSr.AddKeyValue("Type", AssetTypeToString(metadata.type));
				assetSr.AddKeyValue("Filepath", metadata.filepath.generic_string());

				assetSr.EndMap();
			}

			assetSr.EndSequence(); // Asset sequence

			assetSr.EndMap(); // End

			assetSr.Serialize();
		}

		// Save dirty assets
		auto &loadedAssets = m_AssetManager->GetLoadedAssets();
		for (auto &[handle, metadata] : assetRegistry)
		{
			auto it = loadedAssets.find(handle);
            if (it != loadedAssets.end())
            {
			    if (it->second && it->second->IsDirty())
			    {
                    it->second->Serialize(metadata.filepath);
                    it->second->SetDirtyFlag(false);
			    }
            }
		}

        return true;
	}

	Ref<Project> Project::Deserialize(const std::filesystem::path &filepath)
	{
		bool exists = std::filesystem::exists(filepath);
		LOG_ASSERT(exists, "[Project Serializer] File does not exists");
		if (!exists)
		{
			return nullptr;
		}

		YAML::Node projectFileNode = Serializer::Deserialize(filepath);
		YAML::Node projectNode = projectFileNode["Project"];

		ProjectInfo info;
		info.name = projectNode["Name"].as<std::string>();
		info.filepath = filepath;
		info.assetDirectory = projectNode["AssetPath"].as<std::string>();
		info.assetRegistryFilepath = projectNode["AssetRegistry"].as<std::string>();
		info.defaultSceneHandle = AssetHandle(projectNode["DefaultSceneHandle"].as<uint64_t>());
		info.scriptModuleFilepath = projectNode["ScriptModule"].as<std::string>();

		Ref<Project> project = Project::Create(info);

		auto &assetManager = project->GetAssetManager();

		// import registry
		if (!info.assetRegistryFilepath.empty())
		{
			// project filepath / asset filename (.ixreg)
			std::filesystem::path assetRegFilepath = filepath.parent_path() / info.assetRegistryFilepath;
			YAML::Node assetRegFileNode = Serializer::Deserialize(assetRegFilepath);
			YAML::Node assetRegNode = assetRegFileNode["AssetRegistry"];

			for (YAML::Node assetNode : assetRegNode["Assets"])
			{
				AssetHandle handle = AssetHandle(assetNode["Handle"].as<uint64_t>());
				AssetMetaData metadata;
				metadata.type = AssetTypeFromString(assetNode["Type"].as<std::string>());
				metadata.filepath = assetNode["Filepath"].as<std::string>();

				assetManager.AssignMetaData(handle, metadata);
			}
		}

		return project;
	}

	Project *Project::GetInstance()
    {
        return project;
    }

    Ref<Project> Project::Create(const ProjectInfo &info)
    {
        return CreateRef<Project>(info);
    }

    bool Project::BuildSolution()
    {
		m_Info.scriptModuleFilepath = std::format("Bin/{}.dll", m_Info.name);
		bool appAssemblyAvailable = std::filesystem::exists(GetScriptModulePath());

        if (!appAssemblyAvailable)
        {
		    // restore NuGet
            {
			    std::string buildCommand = "msbuild \"" + GetSolutionFilepath().generic_string() + "\" /t:Restore /p:Configuration=Release /p:Platform=\"Any CPU\"";
			    std::system(buildCommand.c_str());
		    }

            // Build
            {
			    std::string buildCommand = "msbuild \"" + GetSolutionFilepath().generic_string() + "\" /p:Configuration=Release /p:Platform=\"Any CPU\"";
			    std::system(buildCommand.c_str());
            }
        }
        
        m_Info.scriptModuleFilepath = std::format("Bin/{}.dll", m_Info.name);
        appAssemblyAvailable = std::filesystem::exists(GetScriptModulePath());

        // Validate .dll file
        LOG_ASSERT(appAssemblyAvailable, "[Project] Failed to build Solution");
        return appAssemblyAvailable;
    }

	void Project::CreateDirectories()
	{
		std::filesystem::path projectDir = GetDirectory();

		// Create asset directory
		std::filesystem::path assetDirectory = GetAssetDirectory();
		if (!std::filesystem::exists(assetDirectory))
			std::filesystem::create_directories(assetDirectory);

		// Create script directory
		std::filesystem::path scriptDirectory = GetScriptsDirectory();
		if (!std::filesystem::exists(scriptDirectory))
			std::filesystem::create_directories(scriptDirectory);
	}

	void Project::CopyDependencies()
	{
		// copy IgniteScriptEngine.dll to project dir
		std::filesystem::path projectBinDir = GetScriptBinDirectory();
		if (!std::filesystem::exists(projectBinDir))
		{
			std::filesystem::create_directory(projectBinDir);
		}

        static std::array<std::string, 5> dependencies =
        {
            "IgniteScriptEngine.dll",
            "IgniteScriptEngine.deps.json",
            "MochiSharp.Managed.dll",
            "MochiSharp.Managed.deps.json",
            "MochiSharp.Managed.runtimeconfig.json"
        };

        // Candidate source directories to search for dependencies. Prefer the executable directory.
        std::filesystem::path exeDir = GetExecutableDirectory();
        std::vector<std::filesystem::path> candidates = {
            exeDir,
            exeDir / "bin",
            exeDir / "bin" / "Debug",
            exeDir / "bin" / "Release",
            exeDir / "Bin",
            exeDir / "Bin" / "Debug",
            exeDir / "Bin" / "Release"
        };

        bool dependenciesAvailable = false;
        for (auto &dep : dependencies)
        {
            std::filesystem::path targetDepFilename = projectBinDir / dep;
            // if already copied, skip
            if (std::filesystem::exists(targetDepFilename))
            {
                dependenciesAvailable = true;
                LOG_INFO("[Project] Script dependency \"{}\" available.", dep);
                continue;
            }

            // find dependency in candidate dirs
            bool copied = false;
            for (auto &cand : candidates)
            {
                std::filesystem::path depFilename = cand / dep;
                if (std::filesystem::exists(depFilename))
                {
                    try
                    {
						LOG_INFO("[Project] Copying script dependency \"{}\".", dep);

                        std::filesystem::copy_file(depFilename, targetDepFilename, std::filesystem::copy_options::overwrite_existing);
                        copied = true;
                        dependenciesAvailable = true;
                        break;
                    }
                    catch (...) {}
                }
            }

            // if not found, continue to try other deps; final assertion below will fail if none copied
            (void)copied;
        }

        LOG_ASSERT(dependenciesAvailable, "[Project] Failed to copy script dependencies");
	}

	void Project::GenerateProject()
    {
        CreateDirectories();

        // Generate the Visual Studio project if there is no solution file
        std::filesystem::path solutionFilepath = GetSolutionFilepath();
        if (!std::filesystem::exists(solutionFilepath))
        {
            // Create visual studio .slnx
            {
                std::string slnx = s_SlnxTemplate;
#ifdef PLATFORM_WINDOWS
                GUID gidReference;
                HRESULT hCreateGuid = CoCreateGuid(&gidReference);
                std::stringstream guidStream;
                if (SUCCEEDED(hCreateGuid))
                {
                    guidStream << std::uppercase << std::hex << std::setfill('0')
                        << std::setw(8) << gidReference.Data1 << "-"
                        << std::setw(4) << gidReference.Data2 << "-"
                        << std::setw(4) << gidReference.Data3 << "-"
                        << std::setw(2) << static_cast<unsigned short>(gidReference.Data4[0])
                        << std::setw(2) << static_cast<unsigned short>(gidReference.Data4[1]) << "-";
                    for (int i = 2; i < 8; ++i)
                        guidStream << std::setw(2) << static_cast<unsigned short>(gidReference.Data4[i]);
                }
                std::string guidStr = guidStream.str();
                if (guidStr.empty())
                    guidStr = "{00000000-0000-0000-0000-000000000000}";
                stringutils::ReplaceWith(slnx, "{GENERATED_GUID}", guidStr);
#else
                stringutils::ReplaceWith(slnx, "{GENERATED_GUID}", "{00000000-0000-0000-0000-000000000000}");
#endif
                stringutils::ReplaceWith(slnx, "{PROJECT_NAME}", m_Info.name);
                std::ofstream slnOut(solutionFilepath, std::ios::out);
                slnOut << slnx;
                slnOut.close();

				LOG_TRACE("[Project] Generate C# Project GUID {}", guidStr);
            }

            // Create .csproj
            {
                // Build ItemGroup for all .cs files under Scripts directory
                std::filesystem::path scriptsDir = GetScriptsDirectory();
                std::string compileItems;

                for (auto &p : std::filesystem::recursive_directory_iterator(scriptsDir))
                {
                    if (!p.is_regular_file())
                        continue;

                    if (p.path().extension() == ".cs")
                    {
                        // compute path relative to project directory
                        std::filesystem::path rel = std::filesystem::relative(p.path(), GetDirectory());
                        std::string includepath = rel.generic_string();
                        // convert forward slashes to backslashes for csproj
                        std::replace(includepath.begin(), includepath.end(), '/', '\\');
                        compileItems += "  <Compile Include=\"" + includepath + "\" />\n";

                        LOG_TRACE("[Project] Add file {} to C# Project", includepath);
                    }
                }

                // If no .cs files found, create Game.cs using template but do not overwrite if exists
                if (compileItems.empty())
                {
                    std::filesystem::path defaultCSharpScriptFilepath = scriptsDir / "Game.cs";
                    if (!std::filesystem::exists(defaultCSharpScriptFilepath) || std::filesystem::is_empty(scriptsDir))
                    {
                        std::string csharpScriptTemplate = s_CSSharpScriptTemplate;
                        stringutils::ReplaceWith(csharpScriptTemplate, "{PROJECT_NAME}", m_Info.name);
                        std::ofstream out(defaultCSharpScriptFilepath, std::ios::out);
                        out << csharpScriptTemplate;
                        out.close();
                        std::string includepath = (m_Info.scriptsDirectory / "Game.cs").generic_string();
                        std::replace(includepath.begin(), includepath.end(), '/', '\\');
                        compileItems += "  <Compile Include=\"" + includepath + "\" />\n";
                    }
                }

                std::string csproj = s_CSProjTemplate;
                // Insert compile items into the first ItemGroup (the one without condition)
                size_t pos = csproj.find("  <ItemGroup>\n    <Compile Include=\"Scripts\\Game.cs\" />\n  </ItemGroup>");
                if (pos != std::string::npos)
                {
                    std::string newItemGroup = "  <ItemGroup>\n" + compileItems + "  </ItemGroup>";
                    csproj.replace(pos, std::string("  <ItemGroup>\n    <Compile Include=\"Scripts\\Game.cs\" />\n  </ItemGroup>").length(), newItemGroup);
                }

                std::ofstream out(GetDirectory() / (m_Info.name + ".csproj"), std::ios::out);
                out << csproj;
                out.close();
            }

            // Create dummy c# script when there is no scripts (new project)
            std::filesystem::path defaultCSharpScriptFilepath = GetScriptsDirectory() / "Game.cs";
            if (!std::filesystem::exists(defaultCSharpScriptFilepath) || std::filesystem::is_empty(GetScriptsDirectory()))
            {
                LOG_WARN("[Project] Creating default C# script {}", defaultCSharpScriptFilepath.filename().generic_string());

                std::string csharpScriptTemplate = s_CSSharpScriptTemplate;
                stringutils::ReplaceWith(csharpScriptTemplate, "{PROJECT_NAME}", m_Info.name);
                std::fstream outfile = std::fstream(defaultCSharpScriptFilepath, std::ios::out);
                outfile << csharpScriptTemplate;
                outfile.close();
            }
        }

        CopyDependencies();

        BuildSolution();
    }
}
