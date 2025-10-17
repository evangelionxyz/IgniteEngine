project "ZLIB"
    kind "SharedLib"
    language "C"
    architecture "x64"
    staticruntime "off"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    disablewarnings { "4005", "4244" }

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

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        symbols "on"
        optimize "on"

    filter "configurations:Shipping"
        runtime "Release"
        symbols "off"
        optimize "on"