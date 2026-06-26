-- Main Engine Project
workspace "IGN"
    location (path.getabsolute("../"))
    architecture "x64"
    multiprocessorcompile("On")
    configurations {
        "Debug",
        "Release",
        "Shipping"
    }

    local wks_absolute = path.getabsolute("../")
    BUILD_DIR = wks_absolute .. "/bin"
    OUTPUT_DIR = BUILD_DIR .. "/%{cfg.buildcfg}"
    THIRDPARTY_DIR = wks_absolute .. "/thirdparty"
    THIRDPARTY_OUTPUT_DIR = BUILD_DIR .. "/%{cfg.buildcfg}/thirdparty/%{prj.name}"
    INTOUTPUT_DIR = wks_absolute .. "/bin/objs/%{cfg.buildcfg}/%{prj.name}"

    include "thirdparty_scripts/thirdparty.lua"

    include "../ignite/editor/ignite.editor.lua"
    include "../ignite/engine/ignite.engine.lua"
    include "../ignite/test/ignite.test.lua"
    include "../scriptengine/ignite.scriptengine.lua"
    include "mochisharp-native.lua"
    include "mochisharp-managed.lua"

    if not os.getenv("GITHUB_ACTIONS") then
        include "utility_project.lua"
    end


-- Automatically generate MSBuild properties to combat Any CPU mapping bugs for Slnx when forcing x64 workspace architecture
require "vstudio"
premake.override(premake.action, "call", function(base, name)
    base(name)
    for wks in premake.global.eachWorkspace() do
        for prj in premake.workspace.eachproject(wks) do
            if prj.language == "C#" then
                local wksPath = wks.location
                local binPath = path.join(wksPath, "bin")
                -- Calculate relative path to the workspace bin folder
                local relBin = path.getrelative(prj.location, binPath) .. "\\\\"
                local propsFile = path.join(prj.location, "Directory.Build.props")

                local f = io.open(propsFile, "w")
                if f then
                    f:write("<Project>\n")
                    f:write("  <PropertyGroup>\n")
                    f:write("    <BaseOutputPath>$(SolutionDir)Bin</BaseOutputPath>\n")
                    f:write("    <IntermediateOutputPath>$(SolutionDir)Bin/objs/$(MSBuildProjectName)/</IntermediateOutputPath>\n")
                    f:write("    <Nullable>enable</Nullable>\n")
                    f:write("    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n")
                    f:write("    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n")
                    f:write("    <GenerateRuntimeConfigurationFiles>true</GenerateRuntimeConfigurationFiles>\n")
                    f:write("  </PropertyGroup>\n")
                    f:write("</Project>\n")
                    f:close()
                end
            end
        end
    end
end)
