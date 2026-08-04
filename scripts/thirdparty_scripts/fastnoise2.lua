project "FastNoise2"
    location (THIRDPARTY_DIR)
    kind "Makefile"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{FASTNOISE2_SOURCE_DIR}/src/**.cpp",
        "%{FASTNOISE2_SOURCE_DIR}/src/**.h",
        "%{FASTNOISE2_SOURCE_DIR}/src/**.inl",
        "%{FASTNOISE2_SOURCE_DIR}/include/**.h",
    }

    --linux
    filter "system:linux"
        pic "on"

    --windows
    filter "system:windows"
        systemversion "latest"

    filter { "configurations:Debug*" }
        runtime "debug"
        symbols "on"
        buildcommands {
            'cmake -S "%{FASTNOISE2_SOURCE_DIR}" -B "%{FASTNOISE2_SOURCE_DIR}/build" -DBUILD_SHARED_LIBS=ON -DFASTNOISE2_STANDALONE_PROJECT=ON -DFASTNOISE2_TOOLS=OFF -DFASTNOISE2_TESTS=OFF -DCMAKE_BUILD_TYPE=Debug',
            'cmake --build "%{FASTNOISE2_SOURCE_DIR}/build" --config "Debug"',
        }

    filter { "configurations:Release*" }
        runtime "Release"
        optimize "on"

        buildcommands {
            'cmake -S "%{FASTNOISE2_SOURCE_DIR}" -B "%{FASTNOISE2_SOURCE_DIR}/build" -DBUILD_SHARED_LIBS=ON -DFASTNOISE2_STANDALONE_PROJECT=ON -DFASTNOISE2_TOOLS=OFF -DFASTNOISE2_TESTS=OFF -DCMAKE_BUILD_TYPE=Release',
            'cmake --build "%{FASTNOISE2_SOURCE_DIR}/build" --config "Release"'
        }

    filter { "configurations:Shipping*" }
        runtime "Release"
        optimize "on"
        symbols "off"

        buildcommands {
            'cmake -S "%{FASTNOISE2_SOURCE_DIR}" -B "%{FASTNOISE2_SOURCE_DIR}/build" -DBUILD_SHARED_LIBS=ON -DFASTNOISE2_STANDALONE_PROJECT=ON -DFASTNOISE2_TOOLS=OFF -DFASTNOISE2_TESTS=OFF -DCMAKE_BUILD_TYPE=Release',
            'cmake --build "%{FASTNOISE2_SOURCE_DIR}/build" --config "Release"'
        }