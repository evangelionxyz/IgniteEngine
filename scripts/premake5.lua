workspace "IGN"
    location "../"
    flags { "MultiProcessorCompile" }
    configurations {
        "Debug",
        "Release",
        "Shipping"
    }

    BUILD_DIR = "%{wks.location}/bin"
    OUTPUT_DIR = "%{BUILD_DIR}/%{cfg.buildcfg}"
    THIRDPARTY_DIR = "%{wks.location}/thirdparty"
    THIRDPARTY_OUTPUT_DIR = "%{BUILD_DIR}/%{cfg.buildcfg}/thirdparty/%{prj.name}"
    INTOUTPUT_DIR = "%{wks.location}/bin/objs/%{cfg.buildcfg}/%{prj.name}"

    include "thirdparty_scripts/thirdparty.lua"
    group "Engine"
        include "../editor/ignite-editor.lua"
        include "../engine/ignite-engine.lua"
        include "../scriptengine/ignite-scriptengine.lua"
        include "mochisharp-managed.lua"
        include "mochisharp-native.lua"
    group ""