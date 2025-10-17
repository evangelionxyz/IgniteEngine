project "IMGUI"
    location (THIRDPARTY_DIR)
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"
    architecture "x64"

    targetdir (THIRDPARTY_OUTPUT_DIR)
    objdir (INTOUTPUT_DIR)

    files {
        "%{THIRDPARTY_DIR}/IMGUI/imgui_demo.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/imgui_draw.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/imgui_tables.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/imgui_widgets.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/imgui.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/backends/imgui_impl_sdl3.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/backends/imgui_impl_sdl3.h",
        "%{THIRDPARTY_DIR}/IMGUI/backends/imgui_impl_vulkan.cpp",
        "%{THIRDPARTY_DIR}/IMGUI/backends/imgui_impl_vulkan.h",
        "%{THIRDPARTY_DIR}/IMGUI/imconfig.h",
        "%{THIRDPARTY_DIR}/IMGUI/imgui.h",
        "%{THIRDPARTY_DIR}/IMGUI/imgui_internal.h",
        "%{THIRDPARTY_DIR}/IMGUI/imstb_rectpack.h",
        "%{THIRDPARTY_DIR}/IMGUI/imstb_textedit.h",
        "%{THIRDPARTY_DIR}/IMGUI/imstb_truetype.h",

        -- include imguizmo src to compile
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImGuizmo.cpp",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImGuizmo.h",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImGradient.cpp",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImGradient.h",
        "%{THIRDPARTY_DIR}/IMGUIZMO/GraphEditor.cpp",
        "%{THIRDPARTY_DIR}/IMGUIZMO/GraphEditor.h",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImCurveEdit.cpp",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImCurveEdit.h",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImSequencer.cpp",
        "%{THIRDPARTY_DIR}/IMGUIZMO/ImSequencer.h",
    }

    includedirs {
        "%{THIRDPARTY_DIR}/IMGUI/",
        "%{THIRDPARTY_DIR}/SDL3/include",
        "%{IncludeDir.NVRHI_VULKAN_HPP}",
    }

    --windows
    filter "system:windows"
        systemversion "latest"
        files {
            "%{THIRDPARTY_DIR}/IMGUI/backends/imgui_impl_dx12.cpp",
            "%{THIRDPARTY_DIR}/IMGUI/backends/imgui_impl_dx12.hpp",
        }