project "Ignite.Engine"
    location "%{wks.location}/ignite/engine"
    kind "SharedLib"
    architecture "x64"
    language "C++"
    cppdialect "C++23"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    pchheader "ignite_pch.hpp"
    pchsource "src/ignite_pch.cpp"
    forceincludes { "ignite_pch.hpp" }

    files {
        "src/ignite_pch.cpp",
        "src/ignite_pch.hpp",
        "src/**.cpp",
        "src/**.hpp",
        "src/**.h",
    }

    includedirs {
        "src",
        "src/ignite",
        "%{wks.location}/ignite/ignite.renderer/src",
        "%{IncludeDir.SDL3}",
        "%{IncludeDir.UmbraShaderCompiler}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.IMGUIZMO}",
        "%{IncludeDir.IMGUI_NODE}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.MONO}",
        "%{IncludeDir.NVRHI}",
        "%{IncludeDir.STB}",
        "%{IncludeDir.NVRHI_VULKAN_HEADERS}",
        "%{IncludeDir.NVRHI_DIRECTX_HEADERS}",
        "%{IncludeDir.VULKAN_SDK}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.JSON}",
        "%{IncludeDir.NUKLEAR}",
        "%{IncludeDir.MochiSharpNative}",
        "%{IncludeDir.Hostfxr}",
        "%{IncludeDir.ASSIMP}",
        "%{IncludeDir.MSDFATLASGEN}",
        "%{IncludeDir.OPENEXR}",
        "%{IncludeDir.IMATH}",
        "%{IncludeDir.MSDFGEN}",
        "%{IncludeDir.FREETYPE}",
        "%{IncludeDir.TRACY}",
        "%{IncludeDir.FBX_SDK}"
    }

    libdirs { "%{cfg.targetdir}" }

    links {
        "IMGUI",
        "BOX2D",
        "STB",
        "JOLT",
        "SPDLOG",
        "TINYGLTF",
        "NVRHI",
        "ZLIB",
        "YAMLCPP",
        "msdf-atlas-gen",
        "msdfgen",
        "freetype",
        "tracy",
        "MochiSharp.Native",
        "UmbraShaderCompiler",
    }

    defines {
        "IGN_DLL_EXPORTS",
        "VULKAN_HPP_NO_SPACESHIP_OPERATOR",
        "JPH_SHARED_LIBRARY",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_OBJECT_STREAM",
    }

    --linux
    filter "system:linux"
        pic "on"
        defines {
            "PLATFORM_LINUX",
            "IGNITE_WITH_VULKAN"
        }
        libdirs {
            "/usr/lib",
            "/usr/local/lib",
            "%{LibraryDir.FMOD_LINUX}"
        }
        includedirs {
            "/usr/include",
            "%{IncludeDir.OPENEXR_LINUX}",
            "%{IncludeDir.IMATH_LINUX}",
            "%{IncludeDir.SDL3_LINUX}"
        }
        links {
            "vulkan",
            "shaderc_shared",
            "spirv-cross-core",
            "spirv-cross-glsl",
            "nethost",
            "SDL3",
            "OpenEXR",
            "Iex",
            "IlmThread",
            "Imath",
            "xml2",
            "pthread",
            "dl",
            "m",
            "rt",
            "glib-2.0"
        }
        -- Set rpath to $ORIGIN so the binary finds .so files next to itself
        linkoptions { "-Wl,-rpath,'$$ORIGIN'" }
        postbuildcommands {
            '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64/libfmod.so" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64/libfmod.so.14" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64/libfmod.so.14.13" "%{cfg.targetdir}"'
        }

    --windows
    filter { "system:windows", "toolset:msc*"}
        disablewarnings { "4099" }
        buildoptions {
            "/utf-8",
            "/bigobj"
        }

    filter "system:windows"
        systemversion "latest"
        links {
            "d3d12.lib",
            "dxgi.lib",
            "d3dcompiler",
            "dxguid",
            "%{Library.winmm}",
            "%{Library.winsock}",
            "%{Library.winversion}",
            "%{Library.bcrypt}",
            "%{Library.vulkan}",
            "%{Library.mono}",
            "%{Library.ASSIMP}",
            "%{Library.FMOD}",
            "%{Library.Iex}",
            "%{Library.OpenEXR}",
            "%{Library.OpenEXRCore}",
            "%{Library.OpenEXRUtil}",
            "%{Library.IlmThread}",
            "%{Library.Imath}",
            "%{Library.SDL3}"
        }
        defines {
            "PLATFORM_WINDOWS",
            "NOMINMAX",
            "IGNITE_WITH_DX12",
            "IGNITE_WITH_VULKAN",
            "_CRT_SECURE_NO_WARNINGS",
            "WIN32",
            "_WINDOWS"
        }

        postbuildcommands {
            '{COPYFILE} "%{LibraryDir.VULKAN_SDK_BIN}/dxcompiler.dll" "%{cfg.targetdir}"',

            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/deflate.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXR-3_4.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRCore-3_4.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRUtil-3_4.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/Iex-3_4.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/IlmThread-3_4.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/OpenJPH/lib/win32/openjph.0.26.dll" "%{cfg.targetdir}"',

            '{COPYFILE} "%{THIRDPARTY_DIR}/ASSIMP/lib/win32/assimp-vc143-mt.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/windows/x64/fmod.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/SDL3/lib/windows/x64/SDL3.dll" "%{cfg.targetdir}"',

            -- Copying dotnet libraries
            '{COPYFILE} "%{THIRDPARTY_DIR}/MochiSharp/ThirdParty/dotnet/host/fxr/9.0.11/x64/nethost.dll\" "%{cfg.targetdir}\"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/MochiSharp/ThirdParty/dotnet/host/fxr/9.0.11/x64/hostfxr.dll\" "%{cfg.targetdir}\"'
        }

        filter "configurations:Debug"
            runtime "Debug"
            symbols "on" -- with debug info
            defines {
                "IGN_DEBUG_BUILD",
                "DEBUG",
                "_DEBUG"
            }

        filter { "system:windows", "configurations:Debug" }
            links {
                "%{Library.ShaderC_Debug}",
                "%{Library.SPIRV_Cross_Core_Debug}",
                "%{Library.SPIRV_Cross_CPP_Debug}",
                "%{Library.SPIRV_Cross_MSL_Debug}",
                "%{Library.SPIRV_Cross_C_Debug}",
                "%{Library.SPIRV_Cross_GLSL_Debug}",
                "%{Library.SPIRV_Cross_HLSL_Debug}",
                "%{Library.SPIRV_Cross_Reflect_Debug}",
                "%{Library.SPIRV_Cross_Util_Debug}",
                "%{Library.SPIRV_Tools_Debug}",
                "%{Library.OpenEXR}",
                "%{Library.OpenEXRCore}",
                "%{Library.OpenEXRUtil}",
                "%{Library.Iex}",
                "%{Library.IlmThread}",
                "%{Library.Imath}",
                "%{Library.FBX_SDK_DEBUG}",
                "%{Library.FBX_XML_DEBUG}",
                "%{Library.FBX_ALEMBIC_DEBUG}"
            }
            postbuildcommands {
                '{COPYFILE} "%{LibraryDir.FBX_SDK}/x64/debug/libfbxsdk.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXR-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRCore-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRUtil-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/Iex-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/IlmThread-3_4.dll" "%{cfg.targetdir}"'
            }

        filter { "system:linux", "configurations:Debug" }
            libdirs { "%{LibraryDir.FBX_SDK_LINUX_DEBUG}" }
            links { "fmodL", "fbxsdk" }
            postbuildcommands {
                '{COPYFILE} "%{FBX_SDK_PATH}/lib/gcc/x64/debug/libfbxsdk.so" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64/libfmodL.so" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64/libfmodL.so.14" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64/libfmodL.so.14.13" "%{cfg.targetdir}"'
            }

        filter "configurations:Release"
            runtime "release"
            optimize "on"
            symbols "on" -- with debug info
            defines {
                "IGN_RELEASE_BUILD",
                "NDEBUG"
            }

        filter { "system:windows", "configurations:Release" }
            links {
                "%{Library.ShaderC}",
                "%{Library.SPIRV_Cross_Core}",
                "%{Library.SPIRV_Cross_CPP}",
                "%{Library.SPIRV_Cross_MSL}",
                "%{Library.SPIRV_Cross_C}",
                "%{Library.SPIRV_Cross_GLSL}",
                "%{Library.SPIRV_Cross_HLSL}",
                "%{Library.SPIRV_Cross_Reflect}",
                "%{Library.SPIRV_Cross_Util}",
                "%{Library.SPIRV_Tools}",
                "%{Library.OpenEXR}",
                "%{Library.OpenEXRCore}",
                "%{Library.OpenEXRUtil}",
                "%{Library.Iex}",
                "%{Library.IlmThread}",
                "%{Library.Imath}",
                "%{Library.FBX_SDK}",
                "%{Library.FBX_XML}",
                "%{Library.FBX_ALEMBIC}"
            }
            postbuildcommands {
                '{COPYFILE} "%{LibraryDir.FBX_SDK}/x64/release/libfbxsdk.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXR-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRCore-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRUtil-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/Iex-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/IlmThread-3_4.dll" "%{cfg.targetdir}"'
            }

        filter { "system:linux", "configurations:Release" }
            libdirs { "%{LibraryDir.FBX_SDK_LINUX_RELEASE}" }
            links { "fmod", "fbxsdk" }
            postbuildcommands {
                '{COPYFILE} "%{FBX_SDK_PATH}/lib/gcc/x64/release/libfbxsdk.so" "%{cfg.targetdir}"'
            }

        filter "configurations:Shipping"
            runtime "release"
            optimize "speed"
            symbols "off" -- without debug info
            defines {
                "IGN_SHIPPING_BUILD",
                "NDEBUG"
            }

        filter { "system:windows", "configurations:Shipping" }
            links {
                "%{Library.ShaderC}",
                "%{Library.SPIRV_Cross_Core}",
                "%{Library.SPIRV_Cross_C}",
                "%{Library.SPIRV_Cross_GLSL}",
                "%{Library.SPIRV_Cross_HLSL}",
                "%{Library.SPIRV_Cross_Reflect}",
                "%{Library.SPIRV_Cross_Util}",
                "%{Library.SPIRV_Tools}",
                "%{Library.OpenEXR}",
                "%{Library.OpenEXRCore}",
                "%{Library.OpenEXRUtil}",
                "%{Library.Iex}",
                "%{Library.IlmThread}",
                "%{Library.Imath}",
                "%{Library.FBX_SDK}",
                "%{Library.FBX_XML}",
                "%{Library.FBX_ALEMBIC}"
            }
            postbuildcommands {
                '{COPYFILE} "%{LibraryDir.FBX_SDK}/x64/release/libfbxsdk.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXR-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRCore-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRUtil-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/Iex-3_4.dll" "%{cfg.targetdir}"',
                '{COPYFILE} "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/IlmThread-3_4.dll" "%{cfg.targetdir}"'
            }

        filter { "system:linux", "configurations:Shipping" }
            libdirs { "%{LibraryDir.FBX_SDK_LINUX_RELEASE}" }
            links { "fmod", "fbxsdk" }
            postbuildcommands {
                '{COPYFILE} "%{FBX_SDK_PATH}/lib/gcc/x64/release/libfbxsdk.so" "%{cfg.targetdir}"'
            }
