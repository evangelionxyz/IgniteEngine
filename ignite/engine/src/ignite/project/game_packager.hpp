// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GAME_PACKAGER_HPP
#define IGN_GAME_PACKAGER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace ignite
{
    class Project;
    enum class ProjectConfiguration;

    struct IGN_API PackageBuildConfig
    {
        std::filesystem::path outputDirectory;
        std::filesystem::path runtimeBinaryDirectory; // Optional explicit source folder containing Ignite.Runtime.exe
        bool overwriteExisting = true;
        bool includeDedicatedServer = false;
        bool includePdbs = false;
        bool buildScriptAssembly = true;
        bool autoBuildMissingRuntime = true;
        std::string executableName; // Defaults to sanitized project name
    };

    struct IGN_API PackageBuildResult
    {
        bool success = false;
        std::string errorMessage;
        std::filesystem::path outputDirectory;
        std::filesystem::path executablePath;
        std::filesystem::path serverExecutablePath;
        size_t filesCopied = 0;
        uint64_t totalBytesCopied = 0;
    };

    class IGN_API GamePackager
    {
    public:
        static std::filesystem::path FindRuntimeBinaryDirectory(const std::filesystem::path &hintDir = {});
        static std::filesystem::path FindRuntimeBinaryDirectory(const std::filesystem::path &hintDir, ProjectConfiguration configuration);
        static bool IsPathEqualOrSubdir(const std::filesystem::path &candidate, const std::filesystem::path &base);
        static std::string SanitizeExecutableName(const std::string &name);
        static PackageBuildResult PackageProject(Project *project, const PackageBuildConfig &config);
    };
}

#endif
