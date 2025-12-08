project "NVRHI"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/NVRHI/src/common/format-info.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/misc.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/state-tracking.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/utils.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/aftermath.cpp",

        "%{THIRDPARTY_DIR}/NVRHI/src/common/sparse-bitset.h",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/state-tracking.h",

        "%{THIRDPARTY_DIR}/NVRHI/include/nvrhi.h",
        "%{THIRDPARTY_DIR}/NVRHI/include/utils.h",
        "%{THIRDPARTY_DIR}/NVRHI/include/validation.h",
        "%{THIRDPARTY_DIR}/NVRHI/include/vulkan.h",

        -- validation layer
        "%{THIRDPARTY_DIR}/NVRHI/src/validation/validation-backend.h",
        "%{THIRDPARTY_DIR}/NVRHI/src/validation/validation-commandlist.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/validation/validation-device.cpp",

        -- Vulkan
        "%{THIRDPARTY_DIR}/NVRHI/include/nvrhi/vulkan.h",
        "%{THIRDPARTY_DIR}/NVRHI/src/vulkan/**.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/vulkan/**.h",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/NVRHI/include/",
        "%{IncludeDir.NVRHI_VULKAN_HEADERS}",
        "%{IncludeDir.NVRHI_DIRECTX_HEADERS}",
    }

    defines {
        "NVRHI_SHARED_LIBRARY_BUILD",
        "NVRHI_WITH_VALIDATION",
        "NVRHI_WITH_VULKAN", 
        "VULKAN_HPP_STORAGE_SHARED",
        "VULKAN_HPP_STORAGE_SHARED_EXPORT",

        -- Vulkan
        "NVRHI_WITH_VULKAN", 
        "VULKAN_HPP_STORAGE_SHARED",
        "VULKAN_HPP_STORAGE_SHARED_EXPORT",
    }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        symbols "off"

    --windows
    filter "system:windows"
        files {
            "%{THIRDPARTY_DIR}/NVRHI/include/nvrhi/d3d12.h",
            "%{THIRDPARTY_DIR}/NVRHI/src/common/dxgi-format.h",
            "%{THIRDPARTY_DIR}/NVRHI/src/common/dxgi-format.cpp",
            "%{THIRDPARTY_DIR}/NVRHI/src/d3d12/**.cpp",
            "%{THIRDPARTY_DIR}/NVRHI/src/d3d12/**.h",
            "%{THIRDPARTY_DIR}/NVRHI/include/common/nvrhiHLSL.h",
        }
        links {
            "d3d12.lib",
            "dxgi.lib",
        }
        defines {
            "NVRHI_WITH_VALIDATION",
            "NVRHI_WITH_DX12",
            "VK_USE_PLATFORM_WIN32_KHR",
            "NOMINMAX",
        }

