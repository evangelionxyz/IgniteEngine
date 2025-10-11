project "STB"
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