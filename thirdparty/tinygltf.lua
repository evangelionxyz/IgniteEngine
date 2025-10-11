project "TINYGLTF"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/TINYGLTF/tinygltf.cpp",
        "%{THIRDPARTY_DIR}/TINYGLTF/include/tinygltf.h"
    }

    includedirs {
        "%{THIRDPARTY_DIR}/STB/include",
        "%{THIRDPARTY_DIR}/TINYGLTF/include",
        "%{THIRDPARTY_DIR}/JSON"
    }

    filter "system:linux"
        pic "On"

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"