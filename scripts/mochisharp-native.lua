project "MochiSharp.Native"
    location "%{THIRDPARTY_DIR}/MochiSharp/MochiSharp.Native"
    kind "StaticLib"
    language "C++"
    cppdialect "c++23"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{prj.location}/Source/**.cpp",
        "%{prj.location}/Source/**.h"
    }

    includedirs {
        "%{IncludeDir.Hostfxr}"
    }

    links {
        "%{Library.Hostfxr}"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        defines {
            "_WINDOWS",
            "_WIN32",
            "WIN32_LEAN_AND_MEAN",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "configurations:Debug"
        runtime "Debug"
        optimize "off"
        symbols "on"
        defines {
            "_DEBUG",
            "MOCHI_DEBUG"
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        symbols "off"
        defines {
            "_NDEBUG",
            "NDEBUG"
        }