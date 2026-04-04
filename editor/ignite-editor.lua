project "IgniteEditor"
    location "%{wks.location}/editor"
    kind "ConsoleApp"
    staticruntime "off"
    architecture "x64"
    language "c++"
    cppdialect "c++23"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "src/**.cpp",
        "src/**.hpp",
        "src/**.h",
    }

    links {
        "IgniteEngine",
        "JOLT",
        "ZLIB",
        "YAMLCPP"
    }

    includedirs {
        "src",
        "%{wks.location}/engine/src",
        "%{IncludeDir.SDL3}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.IMGUIZMO}",
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
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.MSDFATLASGEN}",
        "%{IncludeDir.MSDFGEN}",
        "%{IncludeDir.FREETYPE}",
        "%{IncludeDir.TRACY}",
        "%{IncludeDir.JSON}",
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

    --linux

    --windows
     filter { "system:windows", "toolset:msc*"}
        disablewarnings { "4099" }
        buildoptions {
            "/utf-8"
        }

    filter "system:windows"
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
            "DEBUG",
            "_DEBUG"
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        symbols "on"
        defines {
            "NDEBUG"
        }

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"
        symbols "off"
        defines {
            "NDEBUG"
        }