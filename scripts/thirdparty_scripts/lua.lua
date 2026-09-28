project "Lua"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C"
    cdialect "C17"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/lua/**.c",
        "%{THIRDPARTY_DIR}/lua/**.h",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/lua",
    }

    defines { }

    --linux
    filter "system:linux"
        pic "on"
        defines {
            "LUA_USE_LINUX"
        }
        links { }

    --windows
    filter "system:windows"
        systemversion "latest"

    filter { "system:windows", "toolset:msc*" }
        buildoptions { }

    filter { "configurations:Debug or Debug-Profiling" }
        runtime "debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "Release"
        optimize "on"
        symbols "off"
