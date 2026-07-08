-- Copyright (c) 2022-present Evangelion Manuhutu | ORigin Engine

project "msdfgen"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "c++11"
    staticruntime "off"

    links "freetype"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/core/**.cpp",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/ext/**.cpp",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/lib/**.cpp",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/include",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include"
    }

    filter "system:linux"
        pic "On"

    defines { "MSDF_USE_CPP11" }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "Release"
        optimize "on"
