project "SPDLOG"
    location (THIRDPARTY_DIR)
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    architecture "x64"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/SPDLOG/src/async.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/src/bundled_fmtlib_format.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/src/cfg.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/src/color_sinks.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/src/file_sinks.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/src/spdlog.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/src/stdout_sinks.cpp",
        "%{THIRDPARTY_DIR}/SPDLOG/include/**.h",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/SPDLOG/include"
    }

    defines {
        "SPDLOG_SHARED_LIB",
        "SPDLOG_COMPILED_LIB",
        "spdlog_EXPORTS"
    }

    --windows
    filter "system:windows"
        defines {
            "WIN32", "_WINDOWS", "_UNICODE"
        }

    filter { "system:windows", "toolset:msc*" }
        disablewarnings { "4251", "4275" }
        buildoptions {
            "/utf-8", "/interface"
        }

    filter "configurations:Debug"
        runtime "debug"
        symbols "on"

    filter "configurations:Release"
        runtime "release"
        symbols "off"
        optimize "on"
