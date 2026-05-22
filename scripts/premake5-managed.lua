-- Separated Managed Project
workspace "IGN-Managed"
    location "../"
    architecture "x64"
    multiprocessorcompile("On")
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

    group "Managed"
        include "../scriptengine/ignite-scriptengine.lua"
        include "mochisharp-managed.lua"
    group ""