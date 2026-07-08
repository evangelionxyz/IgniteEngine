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

    includedirs { "%{IncludeDir.UmbraShaderCompiler}", }

    defines { "UMBRACOMPILER_BUILD_SHARED", }

    -- Linux Default
    filter "system:linux"
        systemversion "latest"
        includedirs { "usr/", "%{IncludeDir.VULKAN_SDK}" }
        libdirs { "%{LibraryDir.VULKAN_SDK}" }
        links {
            "vulkan",
            "dxcompiler",
            "shaderc",
            "shaderc_util",
            "spirv-cross-c",
            "spirv-cross-core",
            "spirv-cross-glsl",
            "spirv-cross-hlsl",
            "spirv-cross-msl",
            "spirv-cross-cpp",
            "spirv-cross-reflect",
            "pthread",
            "dl",
            "m",
            "rt",
        }

    -- Windows Default Linking
    filter "system:windows"
        systemversion "latest"
        includedirs { "%{IncludeDir.VULKAN_SDK}", }
        links {
            "d3d12.lib",
            "dxgi.lib",
            "dxcompiler.lib",
            "d3dcompiler",
            "dxguid",
            "%{Library.vulkan}",
        }
        defines {
            "NOMINMAX",
            "WIN32",
            "_WINDOWS",
            "_CRT_SECURE_NO_WARNINGS"
        }

    -- Windows Debug Linking
    filter { "configurations:Debug", "system:windows" }
        links {
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

    filter { "configurations:Debug-Profiling", "system:windows" }
        links {
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

    -- Windows Release Linking
    filter { "configurations:Release", "system:windows" }
        links {
            "%{Library.ShaderC}",
            "%{Library.SPIRV_Cross_C}", -- C API
            "%{Library.SPIRV_Cross_Core}",
            "%{Library.SPIRV_Cross_CPP}",
            "%{Library.SPIRV_Cross_MSL}",
            "%{Library.SPIRV_Cross_GLSL}",
            "%{Library.SPIRV_Cross_HLSL}",
            "%{Library.SPIRV_Cross_Reflect}",
            "%{Library.SPIRV_Cross_Util}",
            "%{Library.SPIRV_Tools}",
        }

    filter { "configurations:Release-Profiling", "system:windows" }
        links {
            "%{Library.ShaderC}",
            "%{Library.SPIRV_Cross_C}", -- C API
            "%{Library.SPIRV_Cross_Core}",
            "%{Library.SPIRV_Cross_CPP}",
            "%{Library.SPIRV_Cross_MSL}",
            "%{Library.SPIRV_Cross_GLSL}",
            "%{Library.SPIRV_Cross_HLSL}",
            "%{Library.SPIRV_Cross_Reflect}",
            "%{Library.SPIRV_Cross_Util}",
            "%{Library.SPIRV_Tools}",
        }

    -- Windows Shipping Linking
    filter { "configurations:Shipping", "system:windows" }
        links {
            "%{Library.ShaderC}",
            "%{Library.SPIRV_Cross_C}", -- C API
            "%{Library.SPIRV_Cross_Core}",
            "%{Library.SPIRV_Cross_CPP}",
            "%{Library.SPIRV_Cross_MSL}",
            "%{Library.SPIRV_Cross_GLSL}",
            "%{Library.SPIRV_Cross_HLSL}",
            "%{Library.SPIRV_Cross_Reflect}",
            "%{Library.SPIRV_Cross_Util}",
            "%{Library.SPIRV_Tools}",
        }

    filter { "configurations:Shipping-Profiling", "system:windows" }
        links {
            "%{Library.ShaderC}",
            "%{Library.SPIRV_Cross_C}", -- C API
            "%{Library.SPIRV_Cross_Core}",
            "%{Library.SPIRV_Cross_CPP}",
            "%{Library.SPIRV_Cross_MSL}",
            "%{Library.SPIRV_Cross_GLSL}",
            "%{Library.SPIRV_Cross_HLSL}",
            "%{Library.SPIRV_Cross_Reflect}",
            "%{Library.SPIRV_Cross_Util}",
            "%{Library.SPIRV_Tools}",
        }

    filter { "configurations:Debug or Debug-Profiling" }
        optimize "off"
        symbols "on"
        defines {
            "_DEBUG"
        }

    filter { "configurations:Release or Release-Profiling" }
        optimize "on"
        symbols "on"
        defines {
            "NDEBUG"
        }

    filter { "configurations:Shipping or Shipping-Profiling" }
        optimize "on"
        symbols "off"
        defines {
            "NDEBUG"
        }
