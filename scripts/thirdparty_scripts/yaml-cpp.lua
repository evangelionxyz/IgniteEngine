project "yaml-cpp"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/YAML/src/**.cpp",
        "%{THIRDPARTY_DIR}/YAML/src/**.h",
        "%{THIRDPARTY_DIR}/YAML/include/yaml-cpp/**.h"
    }

    defines {
         "YAML_BUILD_SHARED_LIBS",
         "yaml_cpp_EXPORTS"
    }

    includedirs {
        "%{THIRDPARTY_DIR}/YAML/include/"
    }

    filter "system:linux"
        pic "On"

    filter "system:windows"
        systemversion "latest"

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