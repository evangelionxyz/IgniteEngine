project "Ignite.Server"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "C++23"
    staticruntime "off"

    targetdir (OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/Ignite-Server/Networking/source/net/**.cpp",
        "%{THIRDPARTY_DIR}/Ignite-Server/Networking/source/net/**.hpp",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/include",
    }

    -- linux
    filter {"system:linux"}
        libdirs { "%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/Linux" }
        links {
            "%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Linux/libGameNetworkingSockets.so",
            "%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Linux/libprotobuf.so.23",
        }

    -- windows
    filter { "configurations:Debug or Debug-Profiling", "system:windows" }
        links {
            "%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Debug/GameNetworkingSockets.lib",
        }

    filter { "configurations:Release or Release-Profiling or Shipping or Shipping-Profilling", "system:windows"}
        links {
            "%{THIRDPARTY_DIR}/Ignite-Server/ThirdParty/GameNetworkingSockets/lib/Windows/Release/GameNetworkingSockets.lib",
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
