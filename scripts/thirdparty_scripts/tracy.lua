project "tracy"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "C++11"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    defines {
        "TRACY_ENABLE",
        "TRACY_NO_SYSTEM_TRACING"
    }

    files {
        "%{THIRDPARTY_DIR}/tracy/public/TracyClient.cpp"
    }

    includedirs {
        "%{THIRDPARTY_DIR}/tracy/public/",
        "%{THIRDPARTY_DIR}/tracy/public/common",
        "%{THIRDPARTY_DIR}/tracy/public/libbacktrace",
        "%{THIRDPARTY_DIR}/tracy/public/tracy"
    }

    --linux
    filter "system:linux"
        pic "on"

    --windows
    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"
