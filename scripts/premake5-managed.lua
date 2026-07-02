-- Separated Managed Project
workspace "IGN-Managed"
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

    group "Managed"
        include "../scriptengine/ignite.scriptengine.lua"
        include "mochisharp-managed.lua"
    group ""