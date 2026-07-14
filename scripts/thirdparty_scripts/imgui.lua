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
        "%{THIRDPARTY_DIR}/ImGui/imgui_demo.cpp",
        "%{THIRDPARTY_DIR}/ImGui/imgui_draw.cpp",
        "%{THIRDPARTY_DIR}/ImGui/imgui_tables.cpp",
        "%{THIRDPARTY_DIR}/ImGui/imgui_widgets.cpp",
        "%{THIRDPARTY_DIR}/ImGui/imgui.cpp",
        "%{THIRDPARTY_DIR}/ImGui/backends/imgui_impl_sdl3.cpp",
        "%{THIRDPARTY_DIR}/ImGui/backends/imgui_impl_sdl3.h",
        "%{THIRDPARTY_DIR}/ImGui/backends/imgui_impl_vulkan.cpp",
        "%{THIRDPARTY_DIR}/ImGui/backends/imgui_impl_vulkan.h",
        "%{THIRDPARTY_DIR}/ImGui/imconfig.h",
        "%{THIRDPARTY_DIR}/ImGui/imgui.h",
        "%{THIRDPARTY_DIR}/ImGui/imgui_internal.h",
        "%{THIRDPARTY_DIR}/ImGui/imstb_rectpack.h",
        "%{THIRDPARTY_DIR}/ImGui/imstb_textedit.h",
        "%{THIRDPARTY_DIR}/ImGui/imstb_truetype.h",

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
        "%{THIRDPARTY_DIR}/ImGui/",
        "%{THIRDPARTY_DIR}/SDL3/include",
        "%{IncludeDir.NVRHI_VULKAN_HEADERS}",
    }

    filter "system:linux"
        pic "On"

    --windows
    filter "system:windows"
        systemversion "latest"
        files {
            "%{THIRDPARTY_DIR}/ImGui/backends/imgui_impl_dx12.cpp",
            "%{THIRDPARTY_DIR}/ImGui/backends/imgui_impl_dx12.hpp",
        }