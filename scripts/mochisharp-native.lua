project "MochiSharp.Native"
    location "%{THIRDPARTY_DIR}/MochiSharp/MochiSharp.Native"
    kind "StaticLib"
    language "C++"
    cppdialect "c++23"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    pchheader "PCH.hpp"
    pchsource "../thirdparty/MochiSharp/MochiSharp.Native/Source/PCH.cpp"
    forceincludes {
        "PCH.hpp"
    }

    files {
        "%{prj.location}/Source/**.cpp",
        "%{prj.location}/Source/**.hpp"
    }

    includedirs {
        "%{prj.location}/Source",
        "%{prj.location}/Source/MochiSharp",
        "%{IncludeDir.Hostfxr}"
    }

    links {
        "%{Library.Hostfxr}"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        defines {
            "_WINDOWS",
            "_WIN32",
            "WIN32_LEAN_AND_MEAN",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "system:linux"
        pic "on"
        defines { "PLATFORM_LINUX" }
        libdirs { "/usr/local/lib" }
        -- nethost is symlinked to /usr/local/lib by the Dockerfile
        -- (linked from the dotnet-sdk-10.0 install path)
        links { "nethost" }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "Debug"
        optimize "off"
        symbols "on"
        defines {
            "_DEBUG",
            "MOCHI_DEBUG"
        }

    filter { "configurations:Release or Release-Profiling or Shipping or Shipping-Profiling" }
        runtime "Release"
        optimize "on"
        symbols "off"
        defines {
            "_NDEBUG",
            "NDEBUG"
        }