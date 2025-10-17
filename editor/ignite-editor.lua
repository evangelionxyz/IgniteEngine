project "IgniteEditor"
    kind "ConsoleApp"
    staticruntime "off"
    architecture "x64"
    language "c++"
    cppdialect "c++20"

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
        "YAMLCPP",
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
        "%{IncludeDir.NVRHI_VULKAN_HPP}",
        "%{IncludeDir.VULKAN_SDK}",
        "%{IncludeDir.SHADERMAKE}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.JSON}",
    }

    defines {
        "NVRHI_SHARED_LIBRARY_INCLUDE",
        "SHADERMAKE_COLORS",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_OBJECT_STREAM",
    }

    --linux

    --windows
    filter "system:windows"
    buildoptions {
        "/utf-8"
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

    filter "configurations:Debug"
    runtime "Debug"
    symbols "on"

    filter "configurations:Debug"
        runtime "Debug"
        optimize "off"
        symbols "on"
        defines {
            "DEBUG",
            "_DEBUG",
        }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        symbols "off"
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