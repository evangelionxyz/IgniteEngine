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

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"