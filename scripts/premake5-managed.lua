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