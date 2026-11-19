// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "game_packager.hpp"

#include "project.hpp"
#include "ignite/core/logger.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace ignite
{
    namespace
    {
        std::filesystem::path GetCurrentExecutableDirectory()
        {
#ifdef _WIN32
            wchar_t buffer[MAX_PATH]{};
            const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            if (len > 0 && len < MAX_PATH)
            {
                return std::filesystem::path(buffer).parent_path();
            }
#endif
            return std::filesystem::current_path();
        }

        std::string ToLowerCopy(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool CaseInsensitiveEqual(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                    return false;
            }
            return true;
        }

        std::filesystem::path NormalizePathForComparison(const std::filesystem::path &p)
        {
            std::error_code ec;
            std::filesystem::path norm = std::filesystem::weakly_canonical(p, ec);
            if (ec || norm.empty())
                norm = std::filesystem::absolute(p, ec);
            return norm.lexically_normal();
        }

        bool PathsEqualCrossPlatform(const std::filesystem::path &a, const std::filesystem::path &b)
        {
            const std::filesystem::path na = NormalizePathForComparison(a);
            const std::filesystem::path nb = NormalizePathForComparison(b);
            std::error_code ec;
            if (std::filesystem::exists(na, ec) && std::filesystem::exists(nb, ec))
            {
                const bool eq = std::filesystem::equivalent(na, nb, ec);
                if (!ec)
                    return eq;
            }
#ifdef _WIN32
            return CaseInsensitiveEqual(na.generic_string(), nb.generic_string());
#else
            return na == nb;
#endif
        }

        std::filesystem::path FindRepoSolutionUpwards(const std::filesystem::path &startDir)
        {
            std::error_code ec;
            std::filesystem::path cur = startDir;
            for (int i = 0; i < 6 && !cur.empty(); ++i)
            {
                const std::filesystem::path slnx = cur / "IGN.slnx";
                if (std::filesystem::exists(slnx, ec))
                    return slnx;
                cur = cur.parent_path();
            }
            return {};
        }

        std::filesystem::path FindMSBuildExecutable()
        {
#ifdef _WIN32
            std::error_code ec;
            static constexpr std::array<const char *, 4> kCandidatePaths = {
                "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe",
                "C:/Program Files/Microsoft Visual Studio/18/Professional/MSBuild/Current/Bin/amd64/MSBuild.exe",
                "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/amd64/MSBuild.exe",
                "C:/Program Files/Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/amd64/MSBuild.exe"
            };
            for (const char *p : kCandidatePaths)
            {
                if (std::filesystem::exists(p, ec))
                    return std::filesystem::path(p);
            }

            const std::filesystem::path vswhere = "C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe";
            if (std::filesystem::exists(vswhere, ec))
            {
                FILE *pipe = _popen("\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe", "r");
                if (pipe)
                {
                    char line[1024]{};
                    std::string found;
                    if (fgets(line, sizeof(line), pipe) != nullptr)
                    {
                        found = line;
                        while (!found.empty() && (found.back() == '\n' || found.back() == '\r'))
                            found.pop_back();
                    }
                    _pclose(pipe);
                    if (!found.empty() && std::filesystem::exists(found, ec))
                        return std::filesystem::path(found);
                }
            }
#endif
            return {};
        }

        bool IsExcludedEditorFile(const std::filesystem::path &filePath, bool includePdbs)
        {
            const std::string filename = ToLowerCopy(filePath.filename().string());
            const std::string ext = ToLowerCopy(filePath.extension().string());

            if (!includePdbs && (ext == ".pdb" || ext == ".lib" || ext == ".exp" || ext == ".idb"))
                return true;

            if (ext == ".exe")
                return true; // Runtime/Server exes are copied explicitly under target names

            static constexpr std::array<const char *, 15> kExcludedPrefixes = {
                "ignite.editor",
                "ignite.test",
                "ignite.managed.test",
                "avalonia",
                "dock.avalonia",
                "skiasharp",
                "svg.",
                "excss",
                "shimskiasharp",
                "tmds.dbus",
                "nunit.",
                "harfbuzzsharp",
                "microcom",
                "microsoft.",
                "communitytoolkit"
            };

            for (const char *prefix : kExcludedPrefixes)
            {
                if (filename.starts_with(prefix))
                    return true;
            }

            if (filename == "imgui.ini" || filename == "xmlgoogletestresults.xml")
                return true;

            return false;
        }

        bool IsRuntimeDependencyExtension(const std::filesystem::path &filePath, bool includePdbs)
        {
            const std::string filename = ToLowerCopy(filePath.filename().string());
            const std::string ext = ToLowerCopy(filePath.extension().string());
            if (ext == ".dll")
                return true;
            if (includePdbs && ext == ".pdb")
                return true;
            if (filename.ends_with(".deps.json") || filename.ends_with(".runtimeconfig.json"))
                return true;
            return false;
        }

        bool CopySingleFile(const std::filesystem::path &src, const std::filesystem::path &dst, PackageBuildResult &result, std::error_code &ec)
        {
            if (!std::filesystem::exists(src, ec) || ec)
                return false;

            std::filesystem::create_directories(dst.parent_path(), ec);
            if (ec)
                return false;

            const bool copied = std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec)
            {
                ++result.filesCopied;
                std::error_code sizeEc;
                const auto sz = std::filesystem::file_size(dst, sizeEc);
                if (!sizeEc)
                    result.totalBytesCopied += static_cast<uint64_t>(sz);
            }
            return copied && !ec;
        }

        void CopyDirectoryFiltered(const std::filesystem::path &srcDir,
            const std::filesystem::path &dstDir,
            const std::filesystem::path &forbiddenOutputRoot,
            PackageBuildResult &result,
            std::error_code &ec)
        {
            if (!std::filesystem::exists(srcDir, ec) || !std::filesystem::is_directory(srcDir, ec))
                return;

            std::error_code canonEc;
            const std::filesystem::path canonDstRoot = std::filesystem::weakly_canonical(forbiddenOutputRoot, canonEc);

            std::filesystem::create_directories(dstDir, ec);
            if (ec)
                return;

            for (const auto &entry : std::filesystem::recursive_directory_iterator(srcDir, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                if (ec)
                    break;

                const std::filesystem::path canonEntry = std::filesystem::weakly_canonical(entry.path(), canonEc);
                if (!canonDstRoot.empty() && GamePackager::IsPathEqualOrSubdir(canonEntry, canonDstRoot))
                    continue;

                const std::filesystem::path rel = std::filesystem::relative(entry.path(), srcDir, ec);
                if (ec)
                    continue;

                const std::filesystem::path target = dstDir / rel;
                if (entry.is_directory())
                {
                    std::filesystem::create_directories(target, ec);
                }
                else if (entry.is_regular_file())
                {
                    CopySingleFile(entry.path(), target, result, ec);
                }
            }
        }
    }

    std::string GamePackager::SanitizeExecutableName(const std::string &name)
    {
        std::string out;
        out.reserve(name.size());
        for (unsigned char c : name)
        {
            if (std::isalnum(c) || c == '_' || c == '-' || c == '.')
                out.push_back(static_cast<char>(c));
            else if (std::isspace(c))
                out.push_back('_');
        }
        return out.empty() ? std::string("IgniteGame") : out;
    }

    bool GamePackager::IsPathEqualOrSubdir(const std::filesystem::path &candidate, const std::filesystem::path &base)
    {
        if (candidate.empty() || base.empty())
            return false;

        const std::filesystem::path normCand = NormalizePathForComparison(candidate);
        const std::filesystem::path normBase = NormalizePathForComparison(base);

#ifdef _WIN32
        const std::string candRoot = normCand.root_name().string();
        const std::string baseRoot = normBase.root_name().string();
        if (!CaseInsensitiveEqual(candRoot, baseRoot))
            return false;
#else
        if (normCand.root_name() != normBase.root_name())
            return false;
#endif

        if (PathsEqualCrossPlatform(normCand, normBase))
            return true;

        std::error_code ec;
        const std::filesystem::path rel = std::filesystem::relative(normCand, normBase, ec);
        if (ec || rel.empty())
            return false;

        if (rel == ".")
            return true;

        const std::string firstSeg = rel.begin()->string();
        return firstSeg != ".." && !rel.is_absolute();
    }

    std::filesystem::path GamePackager::FindRuntimeBinaryDirectory(const std::filesystem::path &hintDir)
    {
        return FindRuntimeBinaryDirectory(hintDir, ProjectConfiguration::Debug);
    }

    std::filesystem::path GamePackager::FindRuntimeBinaryDirectory(const std::filesystem::path &hintDir, ProjectConfiguration configuration)
    {
        std::error_code ec;
        const std::string configStr = Project::ConfigurationToString(configuration);
        std::vector<std::filesystem::path> candidates;

        if (!hintDir.empty())
            candidates.push_back(hintDir);

        const std::filesystem::path exeDir = GetCurrentExecutableDirectory();
        const std::filesystem::path cwd = std::filesystem::current_path(ec);

        auto addConfigCandidatesFrom = [&](const std::filesystem::path &start)
        {
            std::filesystem::path cur = start;
            for (int i = 0; i < 6 && !cur.empty(); ++i)
            {
                candidates.push_back(cur / "bin" / configStr);
                cur = cur.parent_path();
            }
        };
        addConfigCandidatesFrom(exeDir);
        addConfigCandidatesFrom(cwd);

        const std::string exeDirName = exeDir.filename().string();
        const bool isStandaloneLayout = !std::filesystem::exists(exeDir.parent_path() / "Debug", ec)
            && !std::filesystem::exists(exeDir.parent_path() / "Release", ec)
            && !std::filesystem::exists(exeDir.parent_path() / "Shipping", ec);
        if (CaseInsensitiveEqual(exeDirName, configStr) || isStandaloneLayout)
        {
            candidates.push_back(exeDir);
        }

        for (const auto &dir : candidates)
        {
            if (!dir.empty() && std::filesystem::exists(dir / "Ignite.Runtime.exe", ec))
            {
                return std::filesystem::weakly_canonical(dir, ec);
            }
        }
        return {};
    }

    PackageBuildResult GamePackager::PackageProject(Project *project, const PackageBuildConfig &config)
    {
        PackageBuildResult result;

        if (!project)
        {
            result.errorMessage = "No active project provided to package.";
            return result;
        }

        if (config.outputDirectory.empty())
        {
            result.errorMessage = "Output directory must not be empty.";
            return result;
        }

        const ProjectConfiguration targetConfig = project->GetConfiguration();
        const std::string configStr = Project::ConfigurationToString(targetConfig);

        std::filesystem::path binDir = FindRuntimeBinaryDirectory(config.runtimeBinaryDirectory, targetConfig);
        if (binDir.empty() && config.autoBuildMissingRuntime && config.runtimeBinaryDirectory.empty())
        {
            std::filesystem::path slnxPath = FindRepoSolutionUpwards(GetCurrentExecutableDirectory());
            if (slnxPath.empty())
                slnxPath = FindRepoSolutionUpwards(std::filesystem::current_path());

            const std::filesystem::path msbuildExe = FindMSBuildExecutable();
            if (!slnxPath.empty() && !msbuildExe.empty())
            {
                const std::string buildCmd = std::format("\"{}\" \"{}\" /p:Configuration={} /p:Platform=x64 /m /v:minimal",
                    msbuildExe.string(), slnxPath.string(), configStr);
                LOG_INFO("[GamePackager] Building '{}' native runtime binaries via MSBuild...", configStr);
                std::system(buildCmd.c_str());
                binDir = FindRuntimeBinaryDirectory(config.runtimeBinaryDirectory, targetConfig);
            }
        }

        if (binDir.empty())
        {
            result.errorMessage = std::format("Could not locate Ignite.Runtime.exe for '{}' configuration.", configStr);
            return result;
        }

        std::error_code ec;
        const std::filesystem::path outDir = NormalizePathForComparison(config.outputDirectory);
        if (outDir.empty())
        {
            result.errorMessage = "Failed to resolve package output directory.";
            return result;
        }

        // Prevent packaging onto the project root or inside project Assets / Scripts / Bin folders
        // Uses root-name-aware check so cross-drive destinations (e.g. D:\Proj -> C:\Out) succeed cleanly.
        const std::filesystem::path canonProjDir = NormalizePathForComparison(project->GetDirectory());
        if (PathsEqualCrossPlatform(outDir, canonProjDir))
        {
            result.errorMessage = "Output directory cannot be the project root directory itself.";
            return result;
        }

        if (IsPathEqualOrSubdir(outDir, project->GetAssetDirectory()) ||
            IsPathEqualOrSubdir(outDir, project->GetScriptsDirectory()) ||
            IsPathEqualOrSubdir(outDir, project->GetScriptBinDirectory()))
        {
            result.errorMessage = "Output directory cannot be inside the project's Assets, Scripts, or Bin directory.";
            return result;
        }

        if (config.buildScriptAssembly && std::filesystem::exists(project->GetSolutionFilepath(), ec))
        {
            project->IsCoreDependenciesUpToDate();
            if (!project->BuildSolution(false) || !std::filesystem::exists(project->GetScriptModulePath(), ec))
            {
                result.errorMessage = std::format("Failed to build C# script assembly for '{}' configuration.", configStr);
                return result;
            }
        }

        result.outputDirectory = outDir;
        if (config.overwriteExisting && std::filesystem::exists(outDir, ec))
        {
            // Keep top folder, overwrite files cleanly
        }
        std::filesystem::create_directories(outDir, ec);
        if (ec)
        {
            result.errorMessage = "Failed to create output directory: " + ec.message();
            return result;
        }

        // 1. Copy runtime executable under project game name
        const ProjectInfo &info = project->GetInfo();
        const std::string exeStem = SanitizeExecutableName(config.executableName.empty() ? info.name : config.executableName);
        const std::filesystem::path dstExe = outDir / (exeStem + ".exe");
        if (!CopySingleFile(binDir / "Ignite.Runtime.exe", dstExe, result, ec))
        {
            result.errorMessage = "Failed to copy Ignite.Runtime.exe: " + ec.message();
            return result;
        }
        result.executablePath = dstExe;

        // Optional dedicated server binary
        if (config.includeDedicatedServer && std::filesystem::exists(binDir / "Ignite.Network.exe", ec))
        {
            const std::filesystem::path dstServerExe = outDir / (exeStem + "_Server.exe");
            if (CopySingleFile(binDir / "Ignite.Network.exe", dstServerExe, result, ec))
            {
                result.serverExecutablePath = dstServerExe;
            }
        }

        // 2. Copy native & managed runtime DLL dependencies
        for (const auto &entry : std::filesystem::directory_iterator(binDir, ec))
        {
            if (ec) break;
            if (!entry.is_regular_file())
                continue;

            const auto &p = entry.path();
            if (!IsRuntimeDependencyExtension(p, config.includePdbs) || IsExcludedEditorFile(p, config.includePdbs))
                continue;

            CopySingleFile(p, outDir / p.filename(), result, ec);
        }

        // Copy hostfxr / runtime native subtree if present
        if (std::filesystem::exists(binDir / "runtimes", ec))
        {
            CopyDirectoryFiltered(binDir / "runtimes", outDir / "runtimes", outDir, result, ec);
        }

        // 3. Copy engine resources/ (shaders, default fonts/textures/UI)
        std::filesystem::path resourcesSrc = binDir / "resources";
        if (!std::filesystem::exists(resourcesSrc, ec))
        {
            const std::filesystem::path editorRes = binDir.parent_path().parent_path() / "ignite" / "editor" / "resources";
            if (std::filesystem::exists(editorRes, ec))
                resourcesSrc = editorRes;
        }
        if (std::filesystem::exists(resourcesSrc, ec))
        {
            CopyDirectoryFiltered(resourcesSrc, outDir / "resources", outDir, result, ec);
        }

        // Also ensure latest authoritative HLSL/HLSLI shaders are synced if running from repo layout
        const std::filesystem::path slnxPath = FindRepoSolutionUpwards(binDir);
        if (!slnxPath.empty())
        {
            const std::filesystem::path repoShaders = slnxPath.parent_path() / "ignite" / "editor" / "resources" / "shaders";
            if (std::filesystem::exists(repoShaders, ec))
            {
                CopyDirectoryFiltered(repoShaders, outDir / "resources" / "shaders", outDir, result, ec);
            }
        }

        // 4. Copy project manifest, asset registry, assets, Lua scripts, and compiled C# scripts
        const std::filesystem::path projFilepath = project->GetFilepath();
        project->Serialize(projFilepath);
        CopySingleFile(projFilepath, outDir / projFilepath.filename(), result, ec);

        const std::filesystem::path regSrc = project->GetDirectory() / info.assetRegistryFilepath;
        if (std::filesystem::exists(regSrc, ec))
        {
            CopySingleFile(regSrc, outDir / info.assetRegistryFilepath, result, ec);
        }

        CopyDirectoryFiltered(project->GetAssetDirectory(), outDir / info.assetDirectory, outDir, result, ec);

        if (std::filesystem::exists(project->GetScriptsDirectory(), ec))
        {
            CopyDirectoryFiltered(project->GetScriptsDirectory(), outDir / info.scriptsDirectory, outDir, result, ec);
        }

        const std::filesystem::path bakedDir = project->GetDirectory() / "Baked";
        if (std::filesystem::exists(bakedDir, ec))
        {
            CopyDirectoryFiltered(bakedDir, outDir / "Baked", outDir, result, ec);
        }

        const std::filesystem::path scriptBinDir = project->GetScriptBinDirectory();
        if (std::filesystem::exists(scriptBinDir, ec))
        {
            // Copy top-level core script engine dependencies
            for (const auto &entry : std::filesystem::directory_iterator(scriptBinDir, ec))
            {
                if (ec) break;
                if (!entry.is_regular_file())
                    continue;
                if (!config.includePdbs && CaseInsensitiveEqual(entry.path().extension().string(), ".pdb"))
                    continue;
                CopySingleFile(entry.path(), outDir / "Bin" / entry.path().filename(), result, ec);
            }

            // Copy compiled script assembly for the active configuration (plus Bin/ root dll fallback if present)
            const std::filesystem::path configScriptBin = scriptBinDir / configStr;
            if (std::filesystem::exists(configScriptBin, ec))
            {
                CopyDirectoryFiltered(configScriptBin, outDir / "Bin" / configStr, outDir, result, ec);
            }
        }

        result.success = std::filesystem::exists(dstExe, ec);
        if (result.success)
        {
            LOG_INFO("[GamePackager] Packaged '{}' -> '{}' ({} files, {:.2f} MB)",
                info.name, outDir.string(), result.filesCopied,
                static_cast<double>(result.totalBytesCopied) / (1024.0 * 1024.0));
        }
        else if (result.errorMessage.empty())
        {
            result.errorMessage = "Package build finished without generating output executable.";
        }

        return result;
    }
}
