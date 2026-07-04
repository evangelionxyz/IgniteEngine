project "ZLIB"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C"
    architecture "x64"
    staticruntime "off"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/ZLIB/**.c",
        "%{THIRDPARTY_DIR}/ZLIB/**.h",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/ZLIB"
    }

    defines {
        "ZLIB_INTERNAL"
    }

    filter "system:linux"
        defines {

        }

    filter "system:windows"
        defines {
            "ZLIB_DLL"
        }
    filter { "system:windows", "toolset:msc*" }
        disablewarnings { "4005", "4244" }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"
        symbols "on"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "release"
        optimize "on"
        symbols "off"