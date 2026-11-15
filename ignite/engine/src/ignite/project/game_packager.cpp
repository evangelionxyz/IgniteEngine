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
                if (!canonDstRoot.empty() && canonEntry == canonDstRoot)
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

    std::filesystem::path GamePackager::FindRuntimeBinaryDirectory(const std::filesystem::path &hintDir)
    {
        std::vector<std::filesystem::path> candidates;
        if (!hintDir.empty())
            candidates.push_back(hintDir);

        const std::filesystem::path exeDir = GetCurrentExecutableDirectory();
        candidates.push_back(exeDir);
        candidates.push_back(std::filesystem::current_path());
        candidates.push_back(std::filesystem::current_path() / "bin" / "Debug");
        candidates.push_back(std::filesystem::current_path() / "bin" / "Release");
        candidates.push_back(exeDir.parent_path().parent_path() / "bin" / "Debug");
        candidates.push_back(exeDir.parent_path().parent_path() / "bin" / "Release");

        std::error_code ec;
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

        const std::filesystem::path binDir = FindRuntimeBinaryDirectory(config.runtimeBinaryDirectory);
        if (binDir.empty())
        {
            result.errorMessage = "Could not locate Ignite.Runtime.exe in binary directory.";
            return result;
        }

        std::error_code ec;
        const std::filesystem::path outDir = std::filesystem::weakly_canonical(config.outputDirectory, ec);
        if (outDir.empty())
        {
            result.errorMessage = "Failed to resolve package output directory.";
            return result;
        }

        // Prevent packaging inside project Assets or Scripts folders
        const std::filesystem::path canonProjDir = std::filesystem::weakly_canonical(project->GetDirectory(), ec);
        const std::filesystem::path relToProj = std::filesystem::relative(outDir, canonProjDir, ec);
        if (!ec && (relToProj.empty() || relToProj == "."))
        {
            result.errorMessage = "Output directory cannot be the project root directory itself.";
            return result;
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

        if (std::filesystem::exists(project->GetScriptBinDirectory(), ec))
        {
            CopyDirectoryFiltered(project->GetScriptBinDirectory(), outDir / "Bin", outDir, result, ec);
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
