project "IgniteEngine"
    location "%{wks.location}/engine"
    kind "StaticLib"
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
        "%{IncludeDir.NVRHI_VULKAN_HPP}",
        "%{IncludeDir.VULKAN_SDK}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.JSON}"
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
    }

    defines {
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
            "%{Library.FMOD}",
            "%{Library.SDL3}",
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
            '{COPYFILE} "%{THIRDPARTY_DIR}/FMOD/lib/windows/x64/fmod.dll" "%{cfg.targetdir}"',
            '{COPYFILE} "%{THIRDPARTY_DIR}/SDL3/lib/windows/x64/SDL3.dll" "%{cfg.targetdir}"'
        }

        filter "configurations:Debug"
            runtime "Debug"
            symbols "on"
            defines {
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
            }
        filter "configurations:Release"
            runtime "release"
            optimize "on"
            symbols "on" -- with debug info
            defines {
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
            }

        filter "configurations:Shipping"
            runtime "release"
            optimize "on"
            symbols "off" -- without debug info
            defines {
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
            }