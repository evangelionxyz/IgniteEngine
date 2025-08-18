# IMGUI + ImGuizmo
add_library(IMGUI STATIC
  IMGUI/imgui_demo.cpp
  IMGUI/imgui_draw.cpp
  IMGUI/imgui_tables.cpp
  IMGUI/imgui_widgets.cpp
  IMGUI/imgui.cpp
  IMGUI/backends/imgui_impl_glfw.cpp
  IMGUI/backends/imgui_impl_vulkan.cpp
  IMGUIZMO/ImGuizmo.cpp
  IMGUIZMO/ImGradient.cpp
  IMGUIZMO/GraphEditor.cpp
  IMGUIZMO/ImCurveEdit.cpp
  IMGUIZMO/ImSequencer.cpp
)
target_include_directories(IMGUI PUBLIC
  ${THIRDPARTY_DIR}/IMGUI
  ${THIRDPARTY_DIR}/GLFW/include
  ${THIRDPARTY_DIR}/NVRHI/thirdparty/Vulkan-Headers/include
)
if(WIN32)
  target_sources(IMGUI PRIVATE
    IMGUI/backends/imgui_impl_dx12.cpp
  )
endif()
set_common_target_options(IMGUI)
set_target_properties(IMGUI PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)