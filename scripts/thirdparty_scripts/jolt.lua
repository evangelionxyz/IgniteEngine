project "JOLT"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    -- if (MSVC)
    --     # MSVC specific option to enable PDB generation
    --     set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG:FASTLINK")
    -- else()
    --     # Clang/GCC option to enable debug symbol generation
    --     set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -g")
    -- endif()

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files{
        "%{THIRDPARTY_DIR}/JOLT/Jolt/**.cpp",
        "%{THIRDPARTY_DIR}/JOLT/Jolt/**.h",
    }

    defines {
        "JPH_SHARED_LIBRARY",
        "JPH_BUILD_SHARED_LIBRARY",
        
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_OBJECT_STREAM",

        "JPH_USE_AVX2",
        "JPH_USE_AVX",
        "JPH_USE_SSE4_1",
        "JPH_USE_SSE4_2",
        "JPH_USE_LZCNT",
        "JPH_USE_TZCNT",
        "JPH_USE_F16C",
        "JPH_USE_FMADD",
    }

    includedirs{
        "%{THIRDPARTY_DIR}/JOLT/"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        optimize "off"
        symbols "on"
        defines {
            "_WINDOWS",
            "_DEBUG",
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