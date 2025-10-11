project "NVRHI_D3D12"
kind "StaticLib"
language "C++"
cppdialect "C++20"
staticruntime "off"
architecture "x64"

targetdir (THIRDPARTY_OUTPUT_DIR)
objdir (INTOUTPUT_DIR)

files {
    "%{THIRDPARTY_DIR}/NVRHI/include/nvrhi/d3d12.h",
    "%{THIRDPARTY_DIR}/NVRHI/src/common/dxgi-format.h",
    "%{THIRDPARTY_DIR}/NVRHI/src/common/dxgi-format.cpp",
    "%{THIRDPARTY_DIR}/NVRHI/src/d3d12/**.cpp",
    "%{THIRDPARTY_DIR}/NVRHI/src/d3d12/**.h",
}

includedirs {
    "%{THIRDPARTY_DIR}/NVRHI/include/",
}

defines {
    "NVRHI_WITH_VALIDATION",
    "NVRHI_WITH_DX12",
}

filter "configurations:Debug"
    runtime "Debug"
    symbols "on"

filter "configurations:Release"
    runtime "Release"
    symbols "off"

--windows
filter "system:windows"
defines {
    "NOMINMAX",
}