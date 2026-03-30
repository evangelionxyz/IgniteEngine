workspace "IGN-Managed"
    location "../"
    multiprocessorcompile("On")
    configurations { "Debug", "Release" }

    BUILD_DIR = "%{wks.location}/bin"
    OUTPUT_DIR = "%{BUILD_DIR}/%{cfg.buildcfg}"
    THIRDPARTY_DIR = "%{wks.location}/thirdparty"
    INTOUTPUT_DIR = "%{wks.location}/bin/objs/%{cfg.buildcfg}/%{prj.name}"

    -- Projects
    include "mochisharp-managed.lua"

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
                    f:write("    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n")
                    f:write("    <BaseOutputPath>" .. relBin .. "</BaseOutputPath>\n")
                    f:write("    <IntermediateOutputPath>" .. relBin .. "objs\\\\$(Configuration)\\\\$(MSBuildProjectName)\\\\</IntermediateOutputPath>\n")
                    f:write("    <GenerateRuntimeConfigurationFiles>true</GenerateRuntimeConfigurationFiles>\n")
                    f:write("  </PropertyGroup>\n")
                    f:write("</Project>\n")
                    f:close()
                end
            end
        end
    end
end)