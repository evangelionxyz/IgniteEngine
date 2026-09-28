project "HarfBuzz"
    kind "StaticLib"
    language "C++"
    cppdialect "c++11"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/harfbuzz/src/**.cc",
        "%{THIRDPARTY_DIR}/harfbuzz/src/**.hh"
    }

    include {
        "%{THIRDPARTY_DIR}/harfbuzz/src/"
    }

    defines {

    }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "Release"
        optimize "on"
        symbols "off"

