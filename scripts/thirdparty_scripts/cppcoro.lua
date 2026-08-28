project "cppcoro"
    location(THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "C++23"
    staticruntime "off"
    architecture "x64"

    targetdir(THIRDPARTY_OUTPUT_DIR)
    objdir(INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/cppcoro/include/**.hpp",
        "%{THIRDPARTY_DIR}/cppcoro/lib/**.cpp",
        "%{THIRDPARTY_DIR}/cppcoro/lib/**.hpp",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/cppcoro/include",
}

defines {
    "_WIN32_WINNT=0x0A00",
}

filter "system:windows"
    systemversion "latest"
    links {
        "ws2_32.lib",
        "mswsock",
        "synchronization",
    }
    buildoptions {
        "/wd4996",
        "/wd4267",
        "/wd4244",
        "/permissive-",
    }
filter {}

filter { "configurations:Debug or Debug-Profiling" }
    runtime "debug"
    symbols "on"

filter { "configurations:Release or Release-Profiling" }
    runtime "release"
    optimize "on"

filter { "configurations:Shipping or Shipping-Profiling" }
    runtime "release"
    optimize "on"
    symbols "off"
