project "JOLT"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    architecture "x64"

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
    }

    includedirs{
        "%{THIRDPARTY_DIR}/JOLT/"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        optimize "off"
        symbols "on"
        defines {
            "_DEBUG",
        }
    filter { "system:windows", "configurations:Debug" }
        defines {
            "_WINDOWS",
        }
        links {
            "ucrtd",
            "vcruntimed",
            "msvcrtd",
        }

    filter "configurations:Release"
        optimize "on"
        symbols "on"
        defines {
            "NDEBUG"
        }
    filter { "system:windows", "configurations:Release" }
        defines {
            "_WINDOWS",
        }
        links {
            "ucrt",
            "vcruntime",
            "msvcrt",
        }

    filter "configurations:Shipping"
        optimize "on"
        symbols "off"
        defines {
            "NDEBUG"
        }
    filter { "system:windows", "configurations:Shipping" }
        defines {
            "_WINDOWS",
        }
        links {
            "ucrt",
            "vcruntime",
            "msvcrt",
        }