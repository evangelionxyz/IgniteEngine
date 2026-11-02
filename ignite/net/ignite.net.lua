project "Ignite.Network"
    location "%{wks.location}/ignite/net"
    kind "ConsoleApp"
    staticruntime "off"
    architecture "x64"
    language "c++"
    cppdialect "c++23"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "src/**.cpp",
        "src/**.hpp",
        "src/**.h",
    }

    links {
        "Ignite.Engine",
        "Ignite.Core",
        "Ignite.Physics",
        "Ignite.Server",

        "JOLT",
        "ZLIB",
        "yaml-cpp",
        "Lua",
        "UmbraShaderCompiler",
        "IMGUI",
        "BOX2D",
        "STB",
        "SPDLOG",
        "TINYGLTF",
        "NVRHI",
        "NRI",
        "msdf-atlas-gen",
        "msdfgen",
        "freetype",
        "tracy",
        "MochiSharp.Native",
    }

    includedirs {
        "src",
        "%{wks.location}/ignite/core/src",
        "%{wks.location}/ignite/engine/src",
        "%{wks.location}/ignite/physics/src",

        "%{IncludeDir.IGNITE_SERVER}",
        "%{IncludeDir.GAME_NETWORKING_SOCKETS}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.NRI}/Include",
        "%{IncludeDir.NRI}/Source",
        "%{IncludeDir.NVRHI}",
        "%{IncludeDir.SDL3}",
        "%{IncludeDir.UmbraShaderCompiler}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.STB}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.VULKAN_HEADERS}",
        "%{IncludeDir.DIRECTX_HEADERS}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.FASTNOISE2}",
        "%{IncludeDir.FASTSIMD_CONFIG}",
        "%{IncludeDir.OPENEXR}",
        "%{IncludeDir.IMATH}",
    }

    libdirs { "%{cfg.targetdir}" }

    defines {
    }

    -- linux
    filter {"system:linux"}
        pic "on"
        libdirs { "%{LibraryDir.GAME_NETWORKING_SOCKETS_LINUX}" }
        links {
            "GameNetworkingSockets"
        }
        postbuildcommands {
            copy_file("%{LibraryDir.GAME_NETWORKING_SOCKETS_LINUX}/libGameNetworkingSockets.so", "%{cfg.targetdir}"),
            copy_file("%{LibraryDir.GAME_NETWORKING_SOCKETS_LINUX}/libprotobuf.so.23", "%{cfg.targetdir}"),
        }

    -- windows
    filter { "system:windows", "toolset:msc*"}
        buildoptions {
            "/utf-8",
            "/bigobj",
        }

    filter "system:windows"
        systemversion "latest"
        links {
            "ignite_core.dll.lib",
            "d3d12.lib",
            "dxgi.lib",
            "d3dcompiler",
            "dxguid",
            "%{Library.winmm}",
            "%{Library.winsock}",
            "%{Library.winversion}",
            "%{Library.bcrypt}",
            "%{Library.vulkan}",
            "%{Library.mono}",
            "%{Library.FMOD}",
            "%{Library.Iex}",
            "%{Library.OpenEXR}",
            "%{Library.OpenEXRCore}",
            "%{Library.IlmThread}",
            "%{Library.OpenEXRUtil}",
        }

    filter { "configurations:Debug or Debug-Profiling", "system:windows" }
        libdirs { "%{wks.location}/crates/target/debug" }
        postbuildcommands {
            copy_file("%{wks.location}/crates/target/debug/ignite_core.dll", "%{cfg.targetdir}"),
            copy_file("%{wks.location}/crates/target/debug/ignite_core.pdb", "%{cfg.targetdir}"),
            copy_file("%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Debug/GameNetworkingSockets.dll", "%{cfg.targetdir}"),
            copy_file("%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Debug/libcrypto-3-x64.dll", "%{cfg.targetdir}"),
            copy_file("%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Debug/libprotobufd.dll", "%{cfg.targetdir}"),
        }

    filter { "configurations:Release or Release-Profiling or Shipping or Shipping-Profiling", "system:windows"}
        libdirs { "%{wks.location}/crates/target/release" }
        postbuildcommands {
            copy_file("%{wks.location}/crates/target/release/ignite_core.dll", "%{cfg.targetdir}"),
            copy_file("%{wks.location}/crates/target/release/ignite_core.pdb", "%{cfg.targetdir}"),
            copy_file("%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Release/GameNetworkingSockets.dll", "%{cfg.targetdir}"),
            copy_file("%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Release/libcrypto-3-x64.dll", "%{cfg.targetdir}"),
            copy_file("%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Release/libprotobuf.dll", "%{cfg.targetdir}"),
        }

    filter { "configurations:Debug or Debug-Profiling"}
        runtime "debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"

    filter { "configurations:Shipping or Shipping-Profiling" }
        runtime "Release"
        optimize "on"
        symbols "off"


project "Ignite.NetTests"
    location "%{wks.location}/ignite/net"
    kind "ConsoleApp"
    staticruntime "off"
    architecture "x64"
    language "c++"
    cppdialect "c++23"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "tests/all_tests.cpp",
    }

    links {
        "Ignite.Engine",
        "Ignite.Core",
        "Ignite.Physics",
        "Ignite.Server",

        "JOLT",
        "ZLIB",
        "yaml-cpp",
        "Lua",
        "UmbraShaderCompiler",
        "IMGUI",
        "BOX2D",
        "STB",
        "SPDLOG",
        "TINYGLTF",
        "NVRHI",
        "NRI",
        "msdf-atlas-gen",
        "msdfgen",
        "freetype",
        "tracy",
        "MochiSharp.Native",
    }

    includedirs {
        "src",
        "%{wks.location}/ignite/core/src",
        "%{wks.location}/ignite/engine/src",
        "%{wks.location}/ignite/physics/src",

        "%{IncludeDir.IGNITE_SERVER}",
        "%{IncludeDir.GAME_NETWORKING_SOCKETS}",
        "%{IncludeDir.SPDLOG}",
        "%{IncludeDir.ENTT}",
        "%{IncludeDir.GLM}",
        "%{IncludeDir.JOLT}",
        "%{IncludeDir.BOX2D}",
        "%{IncludeDir.FMOD}",
        "%{IncludeDir.ZLIB}",
        "%{IncludeDir.YAMLCPP}",
        "%{IncludeDir.NRI}/Include",
        "%{IncludeDir.NRI}/Source",
        "%{IncludeDir.NVRHI}",
        "%{IncludeDir.SDL3}",
        "%{IncludeDir.UmbraShaderCompiler}",
        "%{IncludeDir.IMGUI}",
        "%{IncludeDir.STB}",
        "%{IncludeDir.TINYGLTF}",
        "%{IncludeDir.VULKAN_HEADERS}",
        "%{IncludeDir.DIRECTX_HEADERS}",
        "%{IncludeDir.FILEWATCHER}",
        "%{IncludeDir.FASTNOISE2}",
        "%{IncludeDir.FASTSIMD_CONFIG}",
        "%{IncludeDir.OPENEXR}",
        "%{IncludeDir.IMATH}",
    }

    libdirs { "%{cfg.targetdir}" }

    filter { "system:windows", "toolset:msc*"}
        buildoptions {
            "/utf-8",
            "/bigobj",
        }

    filter "system:windows"
        systemversion "latest"
        links {
            "ignite_core.dll.lib",
            "d3d12.lib",
            "dxgi.lib",
            "d3dcompiler",
            "dxguid",
            "%{Library.winmm}",
            "%{Library.winsock}",
            "%{Library.winversion}",
            "%{Library.bcrypt}",
            "%{Library.vulkan}",
            "%{Library.mono}",
            "%{Library.FMOD}",
            "%{Library.Iex}",
            "%{Library.OpenEXR}",
            "%{Library.OpenEXRCore}",
            "%{Library.IlmThread}",
            "%{Library.OpenEXRUtil}",
        }

    filter { "configurations:Debug or Debug-Profiling", "system:windows" }
        libdirs { "%{wks.location}/crates/target/debug" }

    filter { "configurations:Release or Release-Profiling or Shipping or Shipping-Profiling", "system:windows"}
        libdirs { "%{wks.location}/crates/target/release" }

    filter { "configurations:Debug or Debug-Profiling"}
        runtime "debug"
        symbols "on"

    filter { "configurations:Release or Release-Profiling" }
        runtime "Release"
        optimize "on"
