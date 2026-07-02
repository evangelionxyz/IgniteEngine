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

    filter { "configurations:Debug or Debug-Profiling" }
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

    filter { "configurations:Release or Release-Profiling" }
        optimize "on"
        symbols "on"
        defines {
            "NDEBUG"
        }
    filter { "system:windows", "configurations:Release or Release-Profiling" }
        defines {
            "_WINDOWS",
        }
        links {
            "ucrt",
            "vcruntime",
            "msvcrt",
        }

    filter { "configurations:Shipping or Shipping-Profiling" }
        optimize "on"
        symbols "off"
        defines {
            "NDEBUG"
        }
    filter { "system:windows", "configurations:Shipping or Shipping-Profiling" }
        defines {
            "_WINDOWS",
        }
        links {
            "ucrt",
            "vcruntime",
            "msvcrt",
        }