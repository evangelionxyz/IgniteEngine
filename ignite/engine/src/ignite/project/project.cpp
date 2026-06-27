// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "project.hpp"
#include "ignite/core/string_utils.hpp"
#include "ignite/core/signals/signals.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/scriptable_object.hpp"
#include "ignite/core/platform_utils.hpp"

#include "ignite/serializer/serializer.hpp"

#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <format>
#include <chrono>
#include <thread>
#include <ranges>

namespace ignite
{
    static std::string s_CSSharpScriptTemplate =
R"(using Ignite;
using System;

namespace {PROJECT_NAME};
public class {CLASS_NAME} : Entity
{
    public override void OnCreate()
    {
        // Called when entity created
        Console.WriteLine("Hello From C#!");
    }

    public override void OnDestroy()
    {
        // Called when entity destroyed
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
    <TargetFramework>net10.0</TargetFramework>
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
    <Nullable>disable</Nullable>
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
    <Reference Include="Ignite.ScriptEngine">
      <HintPath>Bin\Ignite.ScriptEngine.dll</HintPath>
    </Reference>
  </ItemGroup>
</Project>
)";

    Project::Project(const ProjectInfo &info)
        : m_Info(info)
    {
        GenerateProject();

        m_AssetManager = new AssetManager(this);
    }

    void Project::InitScriptEngine()
    {
        m_ScriptEngine = new ScriptEngine(this);
    }

    Project::~Project()
    {
        m_CoreDependencyWatchers.clear();
        if (m_ScriptEngine)
            delete m_ScriptEngine;
        if (m_AssetManager)
            delete m_AssetManager;
    }

    ignite::Path Project::GetProjectFilepath(const ignite::Path &filepath) const
    {
        if (filepath.empty())
        {
            return {};
        }

        if (filepath.is_absolute())
        {
            return filepath.lexically_normal();
        }

        return (m_Info.rootDirectory / filepath).lexically_normal();
    }

    ignite::Path Project::GetProjectRelativeFilepath(const ignite::Path &filepath) const
    {
        if (filepath.empty())
        {
            return {};
        }

        const ignite::Path normalizedRoot = m_Info.rootDirectory.lexically_normal();
        const ignite::Path normalizedPath = filepath.lexically_normal();

        if (!normalizedPath.is_absolute())
        {
            return normalizedPath;
        }

        const ignite::Path relativePath = normalizedPath.lexically_relative(normalizedRoot);
        if (!relativePath.empty())
        {
            const auto firstComponent = relativePath.begin();
            if (firstComponent != "..")
            {
                return relativePath;
            }
        }

        return normalizedPath;
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
        AssetRegistry &assetRegistry = m_AssetManager->GetAssetAssetRegistry();

        for (auto it = assetRegistry.begin(); it != assetRegistry.end();)
        {
            const ignite::Path &filepath = GetProjectFilepath(it->second.filepath);
            if (!ignite::Path::exists(filepath))
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

    const std::string Project::GetAssetDisplayName(AssetHandle handle) const
    {
        return m_AssetManager->GetAssetDisplayName(handle);
    }

    bool Project::Serialize(const ignite::Path &filepath)
	{
		Serializer projectSr(filepath);

		projectSr.BeginMap(); // START

		{
			projectSr.BeginMap("Project");

			projectSr.AddKeyValue("Version", Application::GetVersion());
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
			const ignite::Path assetRegFilepath = m_Info.rootDirectory / m_Info.assetRegistryFilepath;
			Serializer assetSr(assetRegFilepath);

			assetSr.BeginMap(); // Start

			assetSr.BeginMap("AssetRegistry");

			assetSr.BeginSequence("Assets"); // Asset sequence
			for (auto &[handle, metadata] : assetRegistry)
			{
                if (metadata.type == AssetType::Invalid)
                    continue;

				assetSr.BeginMap(); // Begin Metadata

                const auto assetRelativePath = GetProjectRelativeFilepath(metadata.filepath);

				assetSr.AddKeyValue("Handle", static_cast<uint64_t>(handle));
				assetSr.AddKeyValue("Type", AssetTypeToString(metadata.type));
				assetSr.AddKeyValue("Filepath", assetRelativePath.generic_string());

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

	Ref<Project> Project::Deserialize(const ignite::Path &filepath)
	{
		bool exists = ignite::Path::exists(filepath);
		LOG_ASSERT(exists, "[Project Serializer] File does not exists {}", filepath.string());
		if (!exists)
		{
			return nullptr;
		}

		YAML::Node projectFileNode = Serializer::Deserialize(filepath);
		YAML::Node projectNode = projectFileNode["Project"];

		ProjectInfo info;
		info.name = projectNode["Name"].as<std::string>();
		info.filepath = filepath;
        info.rootDirectory = filepath.parent_path();
		info.scriptModuleFilepath = projectNode["ScriptModule"].as<std::string>();
		info.assetDirectory = projectNode["AssetPath"].as<std::string>();
		info.assetRegistryFilepath = projectNode["AssetRegistry"].as<std::string>();
		info.defaultSceneHandle = AssetHandle(projectNode["DefaultSceneHandle"].as<uint64_t>());

		Ref<Project> project = Project::Create(info);

		auto assetManager = project->GetAssetManager();

		// import registry
		if (!info.assetRegistryFilepath.empty())
		{
			// project filepath / asset filename (.ixreg)
			ignite::Path assetRegFilepath = info.rootDirectory / info.assetRegistryFilepath;
			YAML::Node assetRegFileNode = Serializer::Deserialize(assetRegFilepath);
			YAML::Node assetRegNode = assetRegFileNode["AssetRegistry"];

			for (YAML::Node assetNode : assetRegNode["Assets"])
			{
				AssetHandle handle = AssetHandle(assetNode["Handle"].as<uint64_t>());
				AssetMetaData metadata;
				metadata.type = AssetTypeFromString(assetNode["Type"].as<std::string>());
				metadata.filepath = assetNode["Filepath"].as<std::string>();

                if (metadata.type == AssetType::Invalid)
                    continue;

				assetManager->AssignMetaData(handle, metadata);
			}
		}

		return project;
	}

    Ref<Project> Project::Create(const ProjectInfo &info)
    {
        return CreateRef<Project>(info);
    }

    bool Project::IsCoreDependenciesUpToDate()
    {
        static std::array<std::string, 5> coreDeps =
        {
            "Ignite.ScriptEngine.dll",
            "Ignite.ScriptEngine.deps.json",
            "MochiSharp.Managed.dll",
            "MochiSharp.Managed.deps.json",
            "MochiSharp.Managed.runtimeconfig.json"
        };

        // Candidate source directories to search for dependencies. Prefer the executable directory.
        const ignite::Path exeDir = vfs::GetExecutableDirectory();
        const ignite::Path projectBinDir = GetScriptBinDirectory();

        std::lock_guard<std::mutex> lock(m_CoreDependencyMutex);

        bool isUpToDate = true;
        for (auto &dep : coreDeps)
        {
            const ignite::Path targetDepFilename = projectBinDir / dep;
            const ignite::Path depFilename = exeDir / dep;
            m_CoreDependenciesPending[dep] = true;
            if (!ignite::Path::exists(depFilename))
            {
                isUpToDate = false;
                m_CoreDependencies[dep] = false;
                continue;
            }

            const auto srcTime = std::filesystem::last_write_time(depFilename.string());
            const auto dstTime = std::filesystem::exists(targetDepFilename.string())
                ? std::filesystem::last_write_time(targetDepFilename.string()) : std::filesystem::file_time_type::min();

            m_CoreDependencies[dep] = srcTime <= dstTime;
            if (!(srcTime <= dstTime))
                isUpToDate = false;
        }

        return isUpToDate;
    }

    void Project::StartCoreDependencyWatchers()
    {
        m_CoreDependencyWatchers.clear();

        const ignite::Path exeDir = vfs::GetExecutableDirectory();
        for (const auto &[dep, upToDate] : m_CoreDependencies)
        {
            const ignite::Path depFilename = exeDir / dep;
            if (!ignite::Path::exists(depFilename))
                continue;

            m_CoreDependencyWatchers.push_back(Path::WatchFile(depFilename, [this](const std::string &path, const filewatch::Event eventType)
            {
                OnCoreDependencyChanged(path, eventType);
            }));
        }
    }

    void Project::OnCoreDependencyChanged(const std::string &path, const filewatch::Event eventType)
    {
        if (eventType != filewatch::Event::added && eventType != filewatch::Event::modified)
            return;

        {
            std::lock_guard<std::mutex> lock(m_CoreDependencyMutex);
            auto it = m_CoreDependenciesPending.find(path);
            if (it == m_CoreDependenciesPending.end() || !it->second)
                return;
            it->second = false;
        }

        AssetWorker::SubmitJob([this, path]()
        {
            using namespace std::chrono_literals;

            const ignite::Path exeDir = vfs::GetExecutableDirectory();
            const ignite::Path sourcePath = exeDir / path;
            if (!ignite::Path::exists(sourcePath))
            {
                LOG_ERROR("[Project] Dependency {} is not found!", sourcePath.generic_string());
                std::lock_guard<std::mutex> lock(m_CoreDependencyMutex);
                m_CoreDependenciesPending[path] = true;
                return;
            }

            const ignite::Path targetPath = GetScriptBinDirectory() / sourcePath.filename();

            for (int i = 0; i < 80; ++i)
            {
                std::error_code ec;
                if (!std::filesystem::exists(sourcePath.string(), ec) || ec)
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                const auto fileSize = std::filesystem::file_size(sourcePath.string(), ec);
                if (ec)
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                std::ifstream stream(sourcePath.string(), std::ios::binary);
                if (!stream.good())
                {
                    std::this_thread::sleep_for(25ms);
                    continue;
                }

                std::this_thread::sleep_for(25ms);
                std::error_code ecAfter;
                const auto fileSizeAfter = std::filesystem::file_size(sourcePath.string(), ecAfter);
                if (!ecAfter && fileSize == fileSizeAfter)
                {
                    break;
                }
            }

            bool success = false;
            try
            {
                std::filesystem::copy_file(sourcePath.string(), targetPath.string(), std::filesystem::copy_options::overwrite_existing);
                LOG_TRACE("[Project] Dependency {} available", sourcePath.generic_string());
                success = true;
            }
            catch (...)
            {
                // Failed to copy
            }

            {
                std::lock_guard<std::mutex> lock(m_CoreDependencyMutex);
                m_CoreDependenciesPending[path] = true;
            }

            if (!success)
                return;

            auto checkPendingDeps = [this]() -> bool
            {
                std::lock_guard<std::mutex> lock(m_CoreDependencyMutex);
                for (const auto &[dep, ready] : m_CoreDependenciesPending)
                {
                    if (!ready)
                        return false;
                }
                return true;
            };

            // Check every deps update
            if (checkPendingDeps())
            {
                Application::SubmitToMainThread([this]()
                {
                    LOG_TRACE("[Project] Dependencies are up to date.");
                    BuildSolution(true);
                });
            }
            
        });
    }

    void Project::BuildSolution(bool forceRebuild)
    {
        AssetWorker::SubmitJob([this, forceRebuild]()
        {
            bool buildSuccess = ignite::Path::exists(GetScriptModulePath());

            if (!buildSuccess || forceRebuild)
            {
                // restore NuGet
                {
                    AssetWorker::ReportStatus("Building Solution - Restore NuGet Packages...", 0.4f);
                    std::string buildCommand = "msbuild \"" + GetSolutionFilepath().generic_string() + "\" /t:Restore /p:Configuration=Release /p:Platform=\"Any CPU\"";
                    std::system(buildCommand.c_str());
                }

                // Build
                {
                    AssetWorker::ReportStatus("Building Solution...", 0.8f);
                    std::string buildCommand = "msbuild \"" + GetSolutionFilepath().generic_string() + "\" /p:Configuration=Release /p:Platform=\"Any CPU\"";
                    std::system(buildCommand.c_str());
                }
            }

            m_Info.scriptModuleFilepath = std::format("Bin/{}.dll", m_Info.name);
            buildSuccess = ignite::Path::exists(GetScriptModulePath());

            // Validate .dll file
            LOG_ASSERT(buildSuccess, "[Project] Failed to build Solution");

            // MAIN THREAD
            // Notify script engine that the build finished; the script engine will
            // load the assembly and then emit SignalType::Project for the editor layer.
            Application::SubmitToMainThread([this, buildSuccess]()
            {
                SignalBus::Emit(SuccessResultSignal{ buildSuccess, SignalType::ScriptEngine });
                AssetWorker::ReportStatus("Project loaded...", 1.0f);
            });
        });
    }

    void Project::CreateCSharpScript(const ignite::Path &filepath)
    {
        if (ignite::Path::exists(filepath))
            return;

        std::string scriptTemplate = s_CSSharpScriptTemplate;
        stringutils::ReplaceWith(scriptTemplate, "{PROJECT_NAME}", m_Info.name);
        
        std::string className = filepath.stem().string();
        stringutils::ReplaceWith(scriptTemplate, "{CLASS_NAME}", className);

        std::ofstream out(filepath, std::ios::out);
        out << scriptTemplate;
        out.close();

        RegenerateCSharpProject();
    }

    void Project::CreateScriptableObject(const std::string &className, const std::string &fileName, const ignite::Path &targetDirectory)
    {
        // Build output path: <targetDirectory>/<fileName>.ixso
        const std::string safeFileName = fileName.empty() ? className : fileName;
        const ignite::Path filepath = targetDirectory / (safeFileName + GetAssetExtensionFromType(AssetType::ScriptableObject));

        // Avoid overwriting - generate a unique name
        ignite::Path outPath = filepath;
        {
            uint32_t suffix = 1;
            while (ignite::Path::exists(outPath))
            {
                outPath = targetDirectory / std::format("{}_{}{}", safeFileName, suffix, GetAssetExtensionFromType(AssetType::ScriptableObject));
                ++suffix;
            }
        }

        // Create and immediately serialize a default ScriptableObject
        auto so = ScriptableObject::Create(className);
        so->Serialize(outPath);
        so->SetReadyFlag(true);

        // Register it in the asset manager
        const auto relPath = GetProjectRelativeFilepath(outPath);
        AssetHandle handle = m_AssetManager->GetAssetHandle(relPath);
        if (handle == AssetHandle(0))
        {
            handle = AssetHandle(); // new UUID
        }
        so->handle = handle;

        AssetMetaData metadata;
        metadata.filepath = relPath;
        metadata.type = AssetType::ScriptableObject;
        m_AssetManager->AssignAsset(handle, so);
        m_AssetManager->AssignMetaData(handle, metadata);

        LOG_INFO("[Project] Created ScriptableObject '{}' at '{}'", className, outPath.generic_string());
    }

    void Project::RegenerateCSharpProject() const
    {
        ignite::Path scriptsDir = GetScriptsDirectory();

        std::string compileItems;
        if (ignite::Path::exists(scriptsDir))
        {
            for (auto &p : std::filesystem::recursive_directory_iterator(scriptsDir.string()))
            {
                if (!p.is_regular_file())
                    continue;

                if (p.path().extension() == ".cs")
                {
                    ignite::Path rel = ignite::Path::relative(p.path().string(), GetDirectory());
                    std::string includepath = rel.generic_string();
                    std::replace(includepath.begin(), includepath.end(), '/', '\\');
                    compileItems += "    <Compile Include=\"" + includepath + "\" />\n";
                }
            }
        }

        std::string csproj = s_CSProjTemplate;
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

	void Project::CreateDirectories() const
	{
		// Create root directory
		if (!ignite::Path::exists(m_Info.rootDirectory))
			ignite::Path::create_directory(m_Info.rootDirectory);

        // Create script bin
        ignite::Path projectBinDir = GetScriptBinDirectory();
        if (!ignite::Path::exists(projectBinDir))
            ignite::Path::create_directory(projectBinDir);

		// Create asset directory
		ignite::Path assetDirectory = GetAssetDirectory();
		if (!ignite::Path::exists(assetDirectory))
			ignite::Path::create_directories(assetDirectory);

		// Create script directory
		ignite::Path scriptDirectory = GetScriptsDirectory();
		if (!ignite::Path::exists(scriptDirectory))
			ignite::Path::create_directories(scriptDirectory);
	}

	void Project::CopyCoreDependencies()
	{
		const ignite::Path projectBinDir = GetScriptBinDirectory();

		// copy Ignite.ScriptEngine.dll to project dir
        // Candidate source directories to search for dependencies. Prefer the executable directory.
        const ignite::Path exeDir = vfs::GetExecutableDirectory();

        bool depAvailable = false;
        for (auto &[dep, upToDate] : m_CoreDependencies)
        {
            const ignite::Path targetDepFilename = projectBinDir / dep;
            const ignite::Path depFilename = exeDir / dep;
            if (!ignite::Path::exists(depFilename))
                continue;

            // Skip copy if the target is newer or equal
            if (upToDate)
            {
                LOG_INFO("[Project] Dependency \"{}\" is up to date.", dep);
                depAvailable = true;
                continue;
            }

            try
            {
                AssetWorker::ReportStatus(std::format("Copying Dependency {}", dep));

                LOG_WARN("[Project] Copying script dependency \"{}\".", dep);
                std::filesystem::copy_file(depFilename.string(), targetDepFilename.string(), std::filesystem::copy_options::overwrite_existing);
                depAvailable = true;
            }
            catch (...) { }
        }

        LOG_ASSERT(depAvailable, "[Project] Failed to copy script dependencies");
	}

	void Project::GenerateProject()
    {
        CreateDirectories();

        m_Info.scriptModuleFilepath = std::format("Bin/{}.dll", m_Info.name);

        // Generate the Visual Studio project if there is no solution file
        ignite::Path solutionFilepath = GetSolutionFilepath();
        if (!ignite::Path::exists(solutionFilepath))
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
                ignite::Path scriptsDir = GetScriptsDirectory();
                std::string compileItems;

                for (auto &p : std::filesystem::recursive_directory_iterator(scriptsDir.string()))
                {
                    if (!p.is_regular_file())
                        continue;

                    if (p.path().extension() == ".cs")
                    {
                        // compute path relative to project directory
                        ignite::Path rel = ignite::Path::relative(p.path().string(), GetDirectory());
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
                    ignite::Path defaultCSharpScriptFilepath = scriptsDir / "Game.cs";
                    if (!ignite::Path::exists(defaultCSharpScriptFilepath) || std::filesystem::is_empty(scriptsDir.string()))
                    {
                        std::string csharpScriptTemplate = s_CSSharpScriptTemplate;
                        stringutils::ReplaceWith(csharpScriptTemplate, "{PROJECT_NAME}", m_Info.name);
                        stringutils::ReplaceWith(csharpScriptTemplate, "{CLASS_NAME}", "Game");
                        std::ofstream out(defaultCSharpScriptFilepath, std::ios::out);
                        out << csharpScriptTemplate;
                        out.close();
                        std::string includepath = (m_Info.scriptsDirectory / "Game.cs").generic_string();
                        std::replace(includepath.begin(), includepath.end(), '/', '\\');
                        compileItems += "    <Compile Include=\"" + includepath + "\" />\n";
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
            ignite::Path defaultCSharpScriptFilepath = GetScriptsDirectory() / "Game.cs";
            if (!ignite::Path::exists(defaultCSharpScriptFilepath) || std::filesystem::is_empty(GetScriptsDirectory().string()))
            {
                LOG_WARN("[Project] Creating default C# script {}", defaultCSharpScriptFilepath.filename().generic_string());

                std::string csharpScriptTemplate = s_CSSharpScriptTemplate;
                stringutils::ReplaceWith(csharpScriptTemplate, "{PROJECT_NAME}", m_Info.name);
                stringutils::ReplaceWith(csharpScriptTemplate, "{CLASS_NAME}", "Game");
                std::fstream outfile = std::fstream(defaultCSharpScriptFilepath, std::ios::out);
                outfile << csharpScriptTemplate;
                outfile.close();
            }
        }

        IsCoreDependenciesUpToDate();
        CopyCoreDependencies();
        StartCoreDependencyWatchers();
    }
}
