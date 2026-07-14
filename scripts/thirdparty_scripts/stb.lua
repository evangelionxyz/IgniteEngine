project "STB"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    architecture "x64"
    staticruntime "off"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/STB/stb_image.cpp"
    }

    includedirs {
        "%{THIRDPARTY_DIR}/STB/include"
    }

    filter "system:linux"
        pic "On"

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