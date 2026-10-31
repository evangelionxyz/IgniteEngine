project "NRI"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        -- Natvis
        "%{THIRDPARTY_DIR}/NRI/nri.natvis",

        -- Config
        "%{THIRDPARTY_DIR}/NRI/Source/NRIConfig.h",

        -- Public Headers
        "%{THIRDPARTY_DIR}/NRI/Include/**.h",

        -- Shared
        "%{THIRDPARTY_DIR}/NRI/Source/Shared/**.cpp",
        "%{THIRDPARTY_DIR}/NRI/Source/Shared/**.hpp",
        "%{THIRDPARTY_DIR}/NRI/Source/Shared/**.h",

        -- Creation
        "%{THIRDPARTY_DIR}/NRI/Source/Creation/Creation.cpp",

        -- NONE Backend
        "%{THIRDPARTY_DIR}/NRI/Source/NONE/ImplNONE.cpp",

        -- Validation Backend
        "%{THIRDPARTY_DIR}/NRI/Source/Validation/**.cpp",
        "%{THIRDPARTY_DIR}/NRI/Source/Validation/**.hpp",
        "%{THIRDPARTY_DIR}/NRI/Source/Validation/**.h",

        -- Vulkan Backend
        "%{THIRDPARTY_DIR}/NRI/Source/VK/**.cpp",
        "%{THIRDPARTY_DIR}/NRI/Source/VK/**.hpp",
        "%{THIRDPARTY_DIR}/NRI/Source/VK/**.h",
    }

    includedirs {
        -- Shared Directory
        "%{THIRDPARTY_DIR}/NRI/Source/Shared",
        "%{THIRDPARTY_DIR}/NRI/Include",

        -- Vulkan
        "%{IncludeDir.VULKAN_HEADERS}",
        "%{IncludeDir.VULKAN_MEMORY_ALLOCATOR}/include",
        "%{IncludeDir.VULKAN_MEMORY_ALLOCATOR}/src",
    }

    defines {
        "NRI_ENABLE_DEBUG_NAMES_AND_ANNOTATIONS",
        "NRI_ENABLE_NONE_SUPPORT",
        "NRI_ENABLE_VALIDATION_SUPPORT",
        "NRI_ENABLE_VK_SUPPORT",
        "NRI_STREAMER_THREAD_SAFE",
    }

    disablewarnings { "4324" }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        symbols "off"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "Release"
        symbols "off"

    -- Windows
    filter "system:windows"
        files {
            -- D3D12 Backend
            "%{THIRDPARTY_DIR}/NRI/Source/D3D12/**.cpp",
            "%{THIRDPARTY_DIR}/NRI/Source/D3D12/**.hpp",
            "%{THIRDPARTY_DIR}/NRI/Source/D3D12/**.h",

            -- Resources
            "%{THIRDPARTY_DIR}/NRI/Resources/NRI.rc",
            "%{THIRDPARTY_DIR}/NRI/Resources/Version.h",
        }
        resincludedirs {
            "%{THIRDPARTY_DIR}/NRI/Resources",
        }
        includedirs {
            "%{IncludeDir.DIRECTX_HEADERS}/directx",
            "%{IncludeDir.DIRECTX_HEADERS}",
            "%{IncludeDir.DIRECTX_MEMORY_ALLOCATOR}/include",
            "%{IncludeDir.DIRECTX_MEMORY_ALLOCATOR}/src",
        }
        links {
            "d3d12.lib",
            "dxgi.lib",
            "dxguid.lib",
        }
        defines {
            "NRI_ENABLE_D3D12_SUPPORT",
            "VK_USE_PLATFORM_WIN32_KHR",
            "WIN32_LEAN_AND_MEAN",
            "_CRT_SECURE_NO_WARNINGS",
            "NOMINMAX",
            'NRI_API=extern "C" __declspec(dllexport)',
        }

    -- Linux
    filter "system:linux"
        links {
            "dl",
            "pthread",
        }
        defines {
            "VK_USE_PLATFORM_XLIB_KHR",
            'NRI_API=extern "C" __attribute__((visibility("default")))',
        }
