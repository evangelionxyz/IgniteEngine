project("UmbraShaderCompiler")
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/UmbraShaderCompiler/Source/Umbra/**.cpp",
        "%{THIRDPARTY_DIR}/UmbraShaderCompiler/Include/Umbra/**.h",
    }

    includedirs {
        "%{IncludeDir.UmbraShaderCompiler}",
    }

    defines {
        "UMBRACOMPILER_BUILD_SHARED"
    }

    filter "system:linux"
        systemversion "latest"
        includedirs {

        }
        links {
            "vulkan",
            "shaderc_shared",
            "spirv-cross-c",
            "spirv-cross-core",
            "spirv-cross-glsl",
            "spirv-cross-hlsl",
            "spirv-cross-msl",
            "spirv-cross-cpp",
            "spirv-cross-reflect",
            "pthread dl m rt",
        }

    filter "system:windows"
        systemversion "latest"
        includedirs {
            "%{IncludeDir.VULKAN_SDK}",
        }
        links {
            "d3d12.lib",
            "dxgi.lib",
            "dxcompiler.lib",
            "d3dcompiler",
            "dxguid",

            "%{Library.vulkan}",
            "%{Library.ShaderC_Debug}",
            "%{Library.SPIRV_Cross_C_Debug}", -- C API
            "%{Library.SPIRV_Cross_Core_Debug}",
            "%{Library.SPIRV_Cross_CPP_Debug}",
            "%{Library.SPIRV_Cross_MSL_Debug}",
            "%{Library.SPIRV_Cross_GLSL_Debug}",
            "%{Library.SPIRV_Cross_HLSL_Debug}",
            "%{Library.SPIRV_Cross_Reflect_Debug}",
            "%{Library.SPIRV_Cross_Util_Debug}",
            "%{Library.SPIRV_Tools_Debug}",
        }
        defines {
            "NOMINMAX",
            "WIN32",
            "_WINDOWS",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "configurations:Debug"
        optimize "off"
        symbols "on"
        defines {
            "_DEBUG"
        }

    filter "configurations:Release"
        optimize "on"
        symbols "on"
        defines {
            "NDEBUG"
        }

    filter "configurations:Shipping"
        optimize "on"
        symbols "off"
        defines {
            "NDEBUG"
        }
