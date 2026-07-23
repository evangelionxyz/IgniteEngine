-- Main Engine Project
workspace "IGN"
    location (path.getabsolute("../"))
    architecture "x64"
    multiprocessorcompile("On")
    configurations {
        "Debug",
        "Release",
        "Shipping",

        "Debug-Profiling",
        "Release-Profiling",
        "Shipping-Profiling"
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
    include "../crates/ignite_rs.lua"
    include "mochisharp-native.lua"
    include "mochisharp-managed.lua"

    if not os.getenv("GITHUB_ACTIONS") then
        include "utility_project.lua"
    end


-- Generate .vcxproj.user for C++ projects that host .NET via hostfxr.
-- Sets debugger type to "Mixed (.NET Core, .NET 5+)" and points the symbol
-- search path at the per-configuration output directory so VS finds both
-- native and managed .pdb files automatically on every F5 launch.
local function writeMixedDebuggerUserFile(prj, wksLocation)
    local configs = {
        { name = "Debug",    platform = "x64" },
        { name = "Release",  platform = "x64" },
        { name = "Shipping", platform = "x64" },
        { name = "Debug-Profiling",    platform = "x64" },
        { name = "Release-Profiling",  platform = "x64" },
        { name = "Shipping-Profiling", platform = "x64" },
    }

    -- Use the actual project name (e.g. "Ignite.Editor") for the filename
    local userFile = path.join(prj.location, prj.name .. ".vcxproj.user")
    local binDir = path.join(wksLocation, "bin")

    local f = io.open(userFile, "w")
    if not f then
        print("[premake] WARNING: could not write " .. userFile)
        return
    end

    f:write("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n")
    f:write("<Project ToolsVersion=\"Current\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n")

    -- ShowAllFiles — editor-wide preference, harmless on Engine too
    f:write("  <PropertyGroup>\n")
    f:write("    <ShowAllFiles>true</ShowAllFiles>\n")
    f:write("  </PropertyGroup>\n")

    for _, cfg in ipairs(configs) do
        local condition = string.format("'$(Configuration)|$(Platform)'=='%s|%s'", cfg.name, cfg.platform)
        -- Absolute symbol search path for this configuration
        local symPath = path.join(binDir, cfg.name)

        f:write(string.format("  <PropertyGroup Condition=\"%s\">\n", condition))
        f:write("    <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>\n")
        --f:write("    <LocalDebuggerDebuggerType>NativeWithManagedCore</LocalDebuggerDebuggerType>\n")
        f:write("    <LocalDebuggerWorkingDirectory>$(ProjectDir)</LocalDebuggerWorkingDirectory>\n")
        f:write(string.format("    <LocalDebuggerSymbolPath>%s</LocalDebuggerSymbolPath>\n", symPath))
        f:write("  </PropertyGroup>\n")
    end

    f:write("</Project>\n")
    f:close()
    print("[premake] Generated " .. userFile)
end


premake.override(premake.action, "call", function(base, name)
    base(name)

    local wksLocation = nil
    for wks in premake.global.eachWorkspace() do
        wksLocation = wks.location

        -- Generate Directory.Build.props for every C# project
        for prj in premake.workspace.eachproject(wks) do
            if prj.language == "C#" then
                local binPath = path.join(wksLocation, "bin")
                local propsFile = path.join(prj.location, "Directory.Build.props")

                local f = io.open(propsFile, "w")
                if f then
                    f:write("<Project>\n")
                    f:write("  <PropertyGroup>\n")
                    f:write("    <BaseOutputPath>$(SolutionDir)Bin</BaseOutputPath>\n")
                    f:write("    <IntermediateOutputPath>$(SolutionDir)Bin/objs/$(MSBuildProjectName)/</IntermediateOutputPath>\n")
                    f:write("    <DebugType>pdbonly</DebugType>\n")
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

        -- Generate .vcxproj.user with MixedDotNetCore debugger for hostfxr projects
        for prj in premake.workspace.eachproject(wks) do
            if prj.name == "Ignite.Editor" or prj.name == "Ignite.Engine" then
                writeMixedDebuggerUserFile(prj, wksLocation)
            end
        end
    end
end)

