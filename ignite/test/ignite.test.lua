project "Ignite.Test"
    location "%{wks.location}/ignite/test"
    kind "ConsoleApp"
    staticruntime "off"
    architecture "x64"
    language "c++"
    cppdialect "c++23"

    targetdir (OUTPUT_DIR .. "/test")
    objdir (INTOUTPUT_DIR .. "/test")

    files {
        "src/**.cpp",
        "src/**.hpp",
        "src/**.h",
    }

    links {
        "gtest",
        "Ignite.Engine",
        "JOLT",
        "ZLIB",
        "YAMLCPP",
        "UmbraShaderCompiler",
        "IMGUI",
        "BOX2D",
        "STB",
        "SPDLOG",
        "TINYGLTF",
        "NVRHI",
        "msdf-atlas-gen",
        "msdfgen",
        "freetype",
        "tracy",
        "MochiSharp.Native",
    }

    includedirs {
        "src",
        "%{wks.location}/ignite/engine/src",
        "%{IncludeDir.SDL3}",
        "%{IncludeDir.UmbraShaderCompiler}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.gtest}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.IMGUIZMO}",
        "%{IncludeDir.IMGUI_NODE}",
        "%{IncludeDir.MONO}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.NVRHI}",
        "%{IncludeDir.STB}",
        "%{IncludeDir.NVRHI_VULKAN_HEADERS}",
        "%{IncludeDir.NVRHI_DIRECTX_HEADERS}",
        "%{IncludeDir.VULKAN_SDK}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.ASSIMP}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.MSDFATLASGEN}",
        "%{IncludeDir.OPENEXR}",
        "%{IncludeDir.IMATH}",
        "%{IncludeDir.MSDFGEN}",
        "%{IncludeDir.FREETYPE}",
        "%{IncludeDir.TRACY}",
        "%{IncludeDir.JSON}",
        "%{IncludeDir.NUKLEAR}",
        "%{IncludeDir.MochiSharpNative}",
        "%{IncludeDir.Hostfxr}"
    }

    defines {
        "VULKAN_HPP_NO_SPACESHIP_OPERATOR",
        "NVRHI_SHARED_LIBRARY_INCLUDE",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_OBJECT_STREAM",
    }

    -- postbuildcommands {
        -- '{COPYDIR} "%{prj.location}/resources" "%{cfg.targetdir}/resources"',
    -- }

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
            "spirv-cross-c",
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
        linkoptions { "-Wl,-rpath,'$$ORIGIN'" }

    filter { "system:linux", "configurations:Debug" }
        libdirs { "%{LibraryDir.FBX_SDK_LINUX_DEBUG}" }
        links { "fmodL", "fbxsdk" }

    filter { "system:linux", "configurations:Release or Shipping" }
        libdirs { "%{LibraryDir.FBX_SDK_LINUX_RELEASE}" }
        links { "fmod", "fbxsdk" }

    --windows
    filter { "system:windows", "toolset:msc*"}
        disablewarnings { "4099" }
        buildoptions {
            "/utf-8"
        }

    filter "system:windows"
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
            "IGNITE_WITH_DX12",
            "IGNITE_WITH_VULKAN",
            "NOMINMAX",
            "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
            "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS",
            "_CRT_SECURE_NO_WARNINGS"
        }

    links { "d3dcompiler", "dxcompiler", "delayimp" }

    filter "configurations:Debug"
        runtime "Debug"
        optimize "off"
        symbols "on"
        defines {
            "IGN_DEBUG_BUILD",
            "DEBUG",
            "_DEBUG"
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        symbols "on"
        defines {
            "IGN_RELEASE_BUILD",
            "NDEBUG"
        }

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"
        symbols "off"
        defines {
            "IGN_SHIPPING_BUILD",
            "NDEBUG"
        }
