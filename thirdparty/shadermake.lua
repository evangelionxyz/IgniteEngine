project "ShaderMake"
    kind "StaticLib"
    language "c++"
    cppdialect "c++20"
    staticruntime "off"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/src/argparse.c",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/src/Compiler.cpp",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/src/Context.cpp",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/src/ShaderBlob.cpp",

        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake/argparse.h",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake/Compiler.h",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake/Context.h",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake/ShaderBlob.h",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake/ShaderMake.h",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake/Timer.h",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/src",
        "%{THIRDPARTY_DIR}/ShaderMake/ShaderMake/include/ShaderMake",
    }

    defines {
        "SHADERMAKE_COLORS"
    }

    filter "system:windows"
        defines {
            "WIN32_LEAN_AND_MEAN",
            "NOMINMAX",
            "_CRT_SECURE_NO_WARNINGS"
        }
        links { "d3dcompiler", "dxcompiler", "delayimp" }

    filter "system:linux"
        links {
            "pthread"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        symbols "off"
        
    filter "configurations:Shipping"
        runtime "Release"
        symbols "off"
        

