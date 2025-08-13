cmake_minimum_required(VERSION 3.23)

project(IgniteEditorProj LANGUAGES C CXX)

add_executable(IgniteEditor)

# Sources
file(GLOB_RECURSE EDITOR_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

target_sources(IgniteEditor PRIVATE ${EDITOR_SRC})

target_link_libraries(IgniteEditor PRIVATE IgniteEngine)

# Include dirs
include(GNUInstallDirs)

target_include_directories(IgniteEditor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/engine/src
  ${IncludeDir_GLFW}
  ${IncludeDir_BOX2D}
  ${IncludeDir_ENTT}
  ${IncludeDir_JOLT}
  ${IncludeDir_GLM}
  ${IncludeDir_IMGUI}
  ${IncludeDir_FMOD}
  ${IncludeDir_IMGUIZMO}
  ${IncludeDir_MONO}
  ${IncludeDir_SPDLOG}
  ${IncludeDir_NVRHI}
  ${IncludeDir_STB}
  ${IncludeDir_NVRHI_VULKAN_HPP}
  ${IncludeDir_VULKAN_SDK}
  ${IncludeDir_SHADERMAKE}
  ${IncludeDir_ASSIMP}
  ${IncludeDir_FILEWATCHER}
  ${IncludeDir_ZLIB}
  ${IncludeDir_YAMLCPP}
)

# Defines

target_compile_definitions(IgniteEditor PRIVATE
  SHADERMAKE_COLORS
  YAML_CPP_STATIC_DEFINE
  JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
  JPH_DEBUG_RENDERER
  JPH_PROFILE_ENABLED
  JPH_OBJECT_STREAM
)

if(WIN32)
  target_compile_definitions(IgniteEditor PRIVATE
    PLATFORM_WINDOWS
    GLFW_EXPOSE_NATIVE_WIN32
    IGNITE_WITH_DX12
    IGNITE_WITH_VULKAN
    NOMINMAX
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
    _CRT_SECURE_NO_WARNINGS
  )
  target_compile_options(IgniteEditor PRIVATE /utf-8)

  # Embed application icon if an ICO file exists at editor/resources/icon.ico
  set(IGNITE_EDITOR_ICON "${CMAKE_SOURCE_DIR}/resources/icon.ico")
  if(EXISTS "${IGNITE_EDITOR_ICON}")
    set(_IGNITE_EDITOR_RC "${CMAKE_CURRENT_BINARY_DIR}/ignite_editor_icon.rc")
    file(WRITE "${_IGNITE_EDITOR_RC}" "IDI_APP_ICON ICON \"${IGNITE_EDITOR_ICON}\"\n")
    target_sources(IgniteEditor PRIVATE "${_IGNITE_EDITOR_RC}")
  endif()
endif()

set_target_properties(IgniteEditor PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)

# In Visual Studio, set the debugger working directory to the executable output folder
if(MSVC)
  # Expands per-config to something like build/bin/Debug
  set_property(TARGET IgniteEditor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:IgniteEditor>")
endif()
