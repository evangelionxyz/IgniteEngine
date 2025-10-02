cmake_minimum_required(VERSION 3.23)

project(thirdparty-libs LANGUAGES C CXX)
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

function(set_common_target_options tgt)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /utf-8)
  endif()
endfunction()

include(zlib.cmake)
include(glfw.cmake)
include(stb.cmake)
include(spdlog.cmake)
include(imgui.cmake)
include(yamlcpp.cmake)
include(assimp.cmake)
include(nvrhi.cmake)
include(shadermake.cmake)
include(jolt.cmake)
include(msdfatlasgen.cmake)
include(msdfgen.cmake)
include(freetype.cmake)
include(tinygltf.cmake)

# BOX2D (C sources)
add_subdirectory(BOX2D)

# Place all third-party targets under a single 'Dependencies' solution folder in Visual Studio
set(_IGN_THIRDPARTY_TARGETS ZLIB GLFW STB SPDLOG IMGUI YAMLCPP ASSIMP JOLT MSDF_ATLAS_GEN MSDFGEN FREETYPE box2d)
foreach(_t IN LISTS _IGN_THIRDPARTY_TARGETS)
  if(TARGET ${_t})
    set_property(TARGET ${_t} PROPERTY FOLDER "Dependencies")
  endif()
endforeach()

# NVIDIA Libraries
set(_IGN_NVRHI_TARGETS NVRHI NVRHI_VULKAN NVRHI_D3D12 ShaderMake)
foreach(_t IN LISTS _IGN_NVRHI_TARGETS)
  if (TARGET ${_t})
    set_property(TARGET ${_t} PROPERTY FOLDER "Dependencies/NVIDIA")
  endif()
endforeach()
