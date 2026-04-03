project "BOX2D"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C"
    cdialect "C17"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/BOX2D/src/**.c",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/BOX2D/include",
    }

    defines {
        "BOX2D_ENABLE_SIMD",
    }

    --linux
    filter "system:linux"
        pic "on"

    --windows
    filter "system:windows"
        systemversion "latest"

    filter { "system:windows", "toolset:msc*" }
        buildoptions {
            "/experimental:c11atomics",
        }

    filter "configurations:Debug"
        runtime "debug"
        symbols "on"

    filter "configurations:Release"
        runtime "release"
        symbols "off"
        optimize "on"