project "msdf-atlas-gen"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "c++17"
    staticruntime "off"

    links "msdfgen"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdf-atlas-gen/**.cpp"
    }

    includedirs {
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdf-atlas-gen",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen",
        "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/include"
    }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "Release"
        optimize "on"