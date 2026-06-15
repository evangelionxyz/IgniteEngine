project "gtest"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "C++23"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/gtest/googletest/src/gtest-all.cc",
        "%{THIRDPARTY_DIR}/gtest/googletest/src/gtest_main.cc"
    }

    includedirs
    {
        "%{THIRDPARTY_DIR}/gtest/googletest",
        "%{THIRDPARTY_DIR}/gtest/googletest/include",
    }

    filter "configurations:Debug"
        runtime "Debug"
        optimize "off"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        symbols "on"

    filter "configurations:Shipping"
        runtime "Release"
        optimize "on"
        symbols "off"
