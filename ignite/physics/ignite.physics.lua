project "Ignite.Physics"
    location "%{wks.location}/ignite/physics"
    kind "SharedLib"
    architecture "x64"
    language "C++"
    cppdialect "C++23"

    vectorextensions "AVX2"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "src/**.cpp",
        "src/**.hpp",
        "src/**.h",
    }

    includedirs {
        "src",
        "src/ignite",
        
        "%{wks.location}/ignite/core/src",
        "%{wks.location}/crates/src/include",

        "%{IncludeDir.GLM}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.SPDLOG}",
    }

    libdirs { "%{cfg.targetdir}" }

    links {
        "Ignite.Core",
        "JOLT",
        "BOX2D",
        "SPDLOG",
    }

    defines {
        "IGN_PHYSICS_DLL_EXPORTS",
        
        "JPH_SHARED_LIBRARY",
        "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
        "JPH_DEBUG_RENDERER",
        "JPH_PROFILE_ENABLED",
        "JPH_OBJECT_STREAM",
        "GLM_FORCE_SSE2"
    }

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
        }
        includedirs {
            "/usr/include",
        }
        links {
            "pthread",
            "dl",
            "m",
            "rt",
            "glib-2.0"
        }
        -- Set rpath to $ORIGIN so the binary finds .so files next to itself
        linkoptions { "-Wl,-rpath,'$$ORIGIN'" }

    --windows
    filter { "system:windows", "toolset:msc*"}
        disablewarnings { "4099" }
        buildoptions {
            "/utf-8",
            "/bigobj"
        }

    filter "system:windows"
        systemversion "latest"
        links {
        }
        defines {
            "PLATFORM_WINDOWS",
            "NOMINMAX",
            "WIN32",
            "_WINDOWS"
        }

        filter "configurations:Debug"
            runtime "Debug"
            symbols "on" -- with debug info
            defines {
                "IGN_DEBUG_BUILD",
                "DEBUG",
                "_DEBUG"
            }

        filter "configurations:Debug-Profiling"
            runtime "Debug"
            symbols "on" -- with debug info
            defines {
                "IGN_ENABLE_TRACY",
                "IGN_DEBUG_BUILD",
                "DEBUG",
                "_DEBUG"
            }

        filter "configurations:Release"
            runtime "release"
            optimize "on"
            symbols "on" -- with debug info
            defines {
                "IGN_RELEASE_BUILD",
                "NDEBUG"
            }

        filter "configurations:Release-Profiling"
            runtime "release"
            optimize "on"
            symbols "on" -- with debug info
            defines {
                "IGN_ENABLE_TRACY",
                "IGN_RELEASE_BUILD",
                "NDEBUG"
            }

        filter "configurations:Shipping"
            runtime "release"
            optimize "speed"
            symbols "off" -- without debug info
            defines {
                "IGN_SHIPPING_BUILD",
                "NDEBUG"
            }
        filter "configurations:Shipping-Profiling"
            runtime "release"
            optimize "speed"
            symbols "off" -- without debug info
            defines {
                "IGN_ENABLE_TRACY",
                "IGN_SHIPPING_BUILD",
                "NDEBUG"
            }
