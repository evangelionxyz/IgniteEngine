project "IgniteEngine"
    location "%{wks.location}/engine"
    kind "StaticLib"
    architecture "x64"
    language "C++"
    cppdialect "C++23"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "src/ignite/**.cpp",
        "src/ignite/**.hpp",
        "src/ignite/**.h",
    }

    includedirs {
        "src",
        "%{IncludeDir.SDL3}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.IMGUIZMO}",
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
        "MochiSharp.Native"
    }

    defines {
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
            "PLATFORM_LINUX"
        }
        libdirs {
            "/usr/lib"
        }
        includedirs {
            "/usr/include"
        }

        links {
            "vulkan",
            "shaderc_shared",
            "spirv-cross-core",
            "spirv-cross-glsl",
            "pthread",
            "dl",
            "m",
            "rt",
            "glib-2.0"
    }

    --windows
    filter { "system:windows", "toolset:msc*"}
        disablewarnings { "4099" }
        buildoptions {
            "/utf-8"
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
            "GLFW_EXPOSE_NATIVE_WIN32",
            "NOMINMAX",
            "IGNITE_WITH_DX12",
            "IGNITE_WITH_VULKAN",
            "_CRT_SECURE_NO_WARNINGS",
            "WIN32",
            "_WINDOWS"
        }

        postbuildcommands {
            '{COPYDIR} "%{wks.location}/resources" "%{cfg.targetdir}/resources"',
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
            symbols "on"
            defines {
                "IGN_DEBUG_BUILD",
                "DEBUG",
                "_DEBUG"
            }
            links {
                "%{Library.ShaderC_Debug}",
                "%{Library.SPIRV_Cross_Debug}",
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
        filter "configurations:Release"
            runtime "release"
            optimize "on"
            symbols "on" -- with debug info
            defines {
                "IGN_RELEASE_BUILD",
                "NDEBUG"
            }
            links {
                "%{Library.ShaderC}",
                "%{Library.SPIRV_Cross}",
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

        filter "configurations:Shipping"
            runtime "release"
            optimize "speed"
            symbols "off" -- without debug info
            defines {
                "IGN_SHIPPING_BUILD",
                "NDEBUG"
            }
            links {
                "%{Library.ShaderC}",
                "%{Library.SPIRV_Cross}",
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