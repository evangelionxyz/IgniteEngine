project "NVRHI"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/NVRHI/src/common/format-info.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/misc.cpp",
        "%{THIRDPARTY_DIR}/NVRHI/src/common/sparse-bitset.cpp",
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
    }

    includedirs {
        "%{THIRDPARTY_DIR}/NVRHI/include/",
    }

    defines {
        "NVRHI_WITH_VALIDATION",
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
        defines {
            "NOMINMAX",
        }
        files {
            "%{THIRDPARTY_DIR}/NVRHI/include/common/nvrhiHLSL.h",
        }

