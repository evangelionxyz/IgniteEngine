cmake_minimum_required(VERSION 3.23)

project(thirdparty-libs LANGUAGES C CXX)
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

function(set_common_target_options tgt)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /utf-8)
  endif()
endfunction()

# ZLIB (custom subset)
add_library(ZLIB STATIC)
file(GLOB ZLIB_SOURCES CONFIGURE_DEPENDS "ZLIB/*.c")
target_sources(ZLIB PRIVATE ${ZLIB_SOURCES})
target_include_directories(ZLIB PUBLIC ${THIRDPARTY_DIR}/ZLIB)
set_common_target_options(ZLIB)

# GLFW (handpicked sources per premake)
add_library(GLFW STATIC
  GLFW/src/context.c
  GLFW/src/init.c
  GLFW/src/input.c
  GLFW/src/monitor.c
  GLFW/src/null_init.c
  GLFW/src/null_joystick.c
  GLFW/src/null_monitor.c
  GLFW/src/null_window.c
  GLFW/src/platform.c
  GLFW/src/window.c
  GLFW/src/vulkan.c
  GLFW/src/osmesa_context.c
  GLFW/src/wgl_context.c
  GLFW/src/egl_context.c
)
target_include_directories(GLFW PUBLIC ${THIRDPARTY_DIR}/GLFW/include)
if(WIN32)
  target_sources(GLFW PRIVATE
    GLFW/src/win32_init.c
    GLFW/src/win32_joystick.c
    GLFW/src/win32_monitor.c
    GLFW/src/win32_module.c
    GLFW/src/win32_time.c
    GLFW/src/win32_thread.c
    GLFW/src/win32_window.c
  )
  target_compile_definitions(GLFW PUBLIC _GLFW_WIN32 _CRT_SECURE_NO_WARNINGS)
else()
  target_sources(GLFW PRIVATE
    GLFW/src/posix_time.c
    GLFW/src/posix_thread.c
    GLFW/src/posix_poll.c
    GLFW/src/posix_module.c
    GLFW/src/linux_joystick.c
    GLFW/src/x11_init.c
    GLFW/src/x11_monitor.c
    GLFW/src/x11_window.c
    GLFW/src/xkb_unicode.c
  )
  target_compile_definitions(GLFW PUBLIC _GLFW_X11)
endif()
set_common_target_options(GLFW)

# STB (single translation unit wrapper)
add_library(STB STATIC STB/stb_image.cpp)
target_include_directories(STB PUBLIC ${THIRDPARTY_DIR}/STB/include)
set_common_target_options(STB)
set_target_properties(STB PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# SPDLOG (compiled lib)
add_library(SPDLOG STATIC
  SPDLOG/src/async.cpp
  SPDLOG/src/bundled_fmtlib_format.cpp
  SPDLOG/src/cfg.cpp
  SPDLOG/src/color_sinks.cpp
  SPDLOG/src/file_sinks.cpp
  SPDLOG/src/spdlog.cpp
  SPDLOG/src/stdout_sinks.cpp
)
target_include_directories(SPDLOG PUBLIC ${THIRDPARTY_DIR}/SPDLOG/include)
target_compile_definitions(SPDLOG PUBLIC SPDLOG_COMPILED_LIB)
if(WIN32)
  target_compile_definitions(SPDLOG PUBLIC WIN32 _WINDOWS _UNICODE)
endif()
set_common_target_options(SPDLOG)
set_target_properties(SPDLOG PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# BOX2D (C sources)
add_subdirectory(BOX2D)

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

# YAMLCPP (amalgam per premake file list)
file(GLOB_RECURSE YAML_SRC CONFIGURE_DEPENDS
  YAML/src/*.cpp
  YAML/src/*.h
)
add_library(YAMLCPP STATIC ${YAML_SRC} YAML/include/yaml-cpp/yaml.h)
target_include_directories(YAMLCPP PUBLIC ${THIRDPARTY_DIR}/YAML/include)
target_compile_definitions(YAMLCPP PUBLIC YAML_CPP_STATIC_DEFINE)
set_common_target_options(YAMLCPP)
set_target_properties(YAMLCPP PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# ASSIMP (custom subset per premake)
file(GLOB_RECURSE ASSIMP_COMMON
  ASSIMP/code/Common/*.cpp
  ASSIMP/code/CApi/*.cpp
)
set(ASSIMP_ZLIB
  ASSIMP/contrib/zlib/adler32.c
  ASSIMP/contrib/zlib/compress.c
  ASSIMP/contrib/zlib/crc32.c
  ASSIMP/contrib/zlib/deflate.c
  ASSIMP/contrib/zlib/gzclose.c
  ASSIMP/contrib/zlib/gzlib.c
  ASSIMP/contrib/zlib/gzread.c
  ASSIMP/contrib/zlib/gzwrite.c
  ASSIMP/contrib/zlib/infback.c
  ASSIMP/contrib/zlib/inffast.c
  ASSIMP/contrib/zlib/inflate.c
  ASSIMP/contrib/zlib/inftrees.c
  ASSIMP/contrib/zlib/trees.c
  ASSIMP/contrib/zlib/uncompr.c
  ASSIMP/contrib/zlib/zutil.c
)
set(ASSIMP_UNZIP
  ASSIMP/contrib/unzip/ioapi.c
  ASSIMP/contrib/unzip/unzip.c
)
file(GLOB ASSIMP_M3D CONFIGURE_DEPENDS ASSIMP/code/AssetLib/M3D/*.cpp)
file(GLOB ASSIMP_MD5 CONFIGURE_DEPENDS ASSIMP/code/AssetLib/MD5/*.cpp)
file(GLOB ASSIMP_IQM CONFIGURE_DEPENDS ASSIMP/code/AssetLib/IQM/*.cpp)
file(GLOB ASSIMP_COLLADA CONFIGURE_DEPENDS ASSIMP/code/AssetLib/Collada/*.cpp)
file(GLOB ASSIMP_OBJ CONFIGURE_DEPENDS ASSIMP/code/AssetLib/Obj/*.cpp)
file(GLOB ASSIMP_POST CONFIGURE_DEPENDS ASSIMP/code/PostProcessing/*.cpp)

set(ASSIMP_ASSETLIB
  ASSIMP/code/AssetLib/FBX/FBXAnimation.cpp
  ASSIMP/code/AssetLib/FBX/FBXBinaryTokenizer.cpp
  ASSIMP/code/AssetLib/FBX/FBXConverter.cpp
  ASSIMP/code/AssetLib/FBX/FBXDeformer.cpp
  ASSIMP/code/AssetLib/FBX/FBXDocument.cpp
  ASSIMP/code/AssetLib/FBX/FBXDocumentUtil.cpp
  ASSIMP/code/AssetLib/FBX/FBXExporter.cpp
  ASSIMP/code/AssetLib/FBX/FBXImporter.cpp
  ASSIMP/code/AssetLib/FBX/FBXMaterial.cpp
  ASSIMP/code/AssetLib/FBX/FBXMeshGeometry.cpp
  ASSIMP/code/AssetLib/FBX/FBXModel.cpp
  ASSIMP/code/AssetLib/FBX/FBXNodeAttribute.cpp
  ASSIMP/code/AssetLib/FBX/FBXParser.cpp
  ASSIMP/code/AssetLib/FBX/FBXProperties.cpp
  ASSIMP/code/AssetLib/FBX/FBXTokenizer.cpp
  ASSIMP/code/AssetLib/FBX/FBXUtil.cpp
  ${ASSIMP_M3D}
  ${ASSIMP_MD5}
  ${ASSIMP_IQM}
  ${ASSIMP_COLLADA}
  ${ASSIMP_OBJ}
  ASSIMP/code/AssetLib/glTF/glTFCommon.cpp
  ASSIMP/code/AssetLib/glTF/glTFImporter.cpp
  ASSIMP/code/AssetLib/glTF2/glTF2Importer.cpp
  ASSIMP/code/Material/MaterialSystem.cpp
  ASSIMP/code/Geometry/GeometryUtils.cpp
  ${ASSIMP_POST}
  ASSIMP/code/Pbrt/PbrtExporter.cpp
)
add_library(ASSIMP STATIC
  ${ASSIMP_COMMON}
  ${ASSIMP_ZLIB}
  ${ASSIMP_UNZIP}
  ${ASSIMP_ASSETLIB}
)
target_include_directories(ASSIMP PUBLIC
  ${THIRDPARTY_DIR}/ASSIMP
  ${THIRDPARTY_DIR}/ASSIMP/code
  ${THIRDPARTY_DIR}/ASSIMP/include
  ${THIRDPARTY_DIR}/ASSIMP/contrib
  ${THIRDPARTY_DIR}/ASSIMP/contrib/zip
  ${THIRDPARTY_DIR}/ASSIMP/contrib/zlib
  ${THIRDPARTY_DIR}/ASSIMP/contrib/unzip
  ${THIRDPARTY_DIR}/ASSIMP/contrib/pugixml/src
  ${THIRDPARTY_DIR}/ASSIMP/contrib/utf8cpp/source
  ${THIRDPARTY_DIR}/ASSIMP/contrib/rapidjson/include
  ${THIRDPARTY_DIR}/ASSIMP/contrib/openddlparser/include
)
# Defines copied from premake
target_compile_definitions(ASSIMP PUBLIC
  RAPIDJSON_HAS_STDSTRING=1
  ASSIMP_BUILD_NO_X_IMPORTER
  ASSIMP_BUILD_NO_3DS_IMPORTER
  ASSIMP_BUILD_NO_MD3_IMPORTER
  ASSIMP_BUILD_NO_MDL_IMPORTER
  ASSIMP_BUILD_NO_MD2_IMPORTER
  ASSIMP_BUILD_NO_PLY_IMPORTER
  ASSIMP_BUILD_NO_ASE_IMPORTER
  ASSIMP_BUILD_NO_AMF_IMPORTER
  ASSIMP_BUILD_NO_HMP_IMPORTER
  ASSIMP_BUILD_NO_SMD_IMPORTER
  ASSIMP_BUILD_NO_MDC_IMPORTER
  ASSIMP_BUILD_NO_STL_IMPORTER
  ASSIMP_BUILD_NO_LWO_IMPORTER
  ASSIMP_BUILD_NO_DXF_IMPORTER
  ASSIMP_BUILD_NO_NFF_IMPORTER
  ASSIMP_BUILD_NO_RAW_IMPORTER
  ASSIMP_BUILD_NO_OFF_IMPORTER
  ASSIMP_BUILD_NO_AC_IMPORTER
  ASSIMP_BUILD_NO_BVH_IMPORTER
  ASSIMP_BUILD_NO_IRRMESH_IMPORTER
  ASSIMP_BUILD_NO_IRR_IMPORTER
  ASSIMP_BUILD_NO_Q3D_IMPORTER
  ASSIMP_BUILD_NO_B3D_IMPORTER
  ASSIMP_BUILD_NO_TERRAGEN_IMPORTER
  ASSIMP_BUILD_NO_CSM_IMPORTER
  ASSIMP_BUILD_NO_3D_IMPORTER
  ASSIMP_BUILD_NO_LWS_IMPORTER
  ASSIMP_BUILD_NO_OGRE_IMPORTER
  ASSIMP_BUILD_NO_OPENGEX_IMPORTER
  ASSIMP_BUILD_NO_MS3D_IMPORTER
  ASSIMP_BUILD_NO_COB_IMPORTER
  ASSIMP_BUILD_NO_BLEND_IMPORTER
  ASSIMP_BUILD_NO_Q3BSP_IMPORTER
  ASSIMP_BUILD_NO_NDO_IMPORTER
  ASSIMP_BUILD_NO_IFC_IMPORTER
  ASSIMP_BUILD_NO_XGL_IMPORTER
  ASSIMP_BUILD_NO_ASSBIN_IMPORTER
  ASSIMP_BUILD_NO_C4D_IMPORTER
  ASSIMP_BUILD_NO_3MF_IMPORTER
  ASSIMP_BUILD_NO_X3D_IMPORTER
  ASSIMP_BUILD_NO_MMD_IMPORTER
  ASSIMP_BUILD_NO_STEP_EXPORTER
  ASSIMP_BUILD_NO_SIB_IMPORTER
  ASSIMP_BUILD_NO_USD_IMPORTER
  ASSIMP_BUILD_NO_PBRT_IMPORTER
)
set_common_target_options(ASSIMP)
set_target_properties(ASSIMP PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# NVRHI core
add_library(NVRHI STATIC
  NVRHI/src/common/format-info.cpp
  NVRHI/src/common/misc.cpp
  NVRHI/src/common/sparse-bitset.cpp
  NVRHI/src/common/state-tracking.cpp
  NVRHI/src/common/utils.cpp
  NVRHI/src/common/aftermath.cpp
  NVRHI/src/validation/validation-commandlist.cpp
  NVRHI/src/validation/validation-device.cpp
)
target_include_directories(NVRHI PUBLIC ${THIRDPARTY_DIR}/NVRHI/include)
# From premake
target_compile_definitions(NVRHI PUBLIC
  NVRHI_WITH_VALIDATION
  NVRHI_WITH_VULKAN
  VULKAN_HPP_STORAGE_SHARED
  VULKAN_HPP_STORAGE_SHARED_EXPORT
)
if(WIN32)
  target_compile_definitions(NVRHI PUBLIC NOMINMAX)
  target_sources(NVRHI PRIVATE NVRHI/include/nvrhi/nvrhiHLSL.h)
endif()
set_common_target_options(NVRHI)
set_target_properties(NVRHI PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# NVRHI Vulkan
add_library(NVRHI_VULKAN STATIC NVRHI/include/nvrhi/vulkan.h)
file(GLOB NVRHI_VK_SRC CONFIGURE_DEPENDS "NVRHI/src/vulkan/*.cpp" "NVRHI/src/vulkan/*.h")
set_source_files_properties(${NVRHI_VK_SRC} PROPERTIES LANGUAGE CXX)
target_sources(NVRHI_VULKAN PRIVATE ${NVRHI_VK_SRC})

target_include_directories(NVRHI_VULKAN PUBLIC
  ${THIRDPARTY_DIR}/NVRHI/include
  ${THIRDPARTY_DIR}/NVRHI/thirdparty/Vulkan-Headers/include
)
# From premake
target_compile_definitions(NVRHI_VULKAN PUBLIC
  NVRHI_WITH_VALIDATION
  NVRHI_WITH_VULKAN
  VULKAN_HPP_STORAGE_SHARED
  VULKAN_HPP_STORAGE_SHARED_EXPORT
  $<$<BOOL:WIN32>:VK_USE_PLATFORM_WIN32_KHR>
  $<$<BOOL:WIN32>:NOMINMAX>
)
set_common_target_options(NVRHI_VULKAN)
set_target_properties(NVRHI_VULKAN PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# NVRHI D3D12
add_library(NVRHI_D3D12 STATIC
  NVRHI/include/nvrhi/d3d12.h
  NVRHI/src/common/dxgi-format.h
  NVRHI/src/common/dxgi-format.cpp
)
file(GLOB NVRHI_D3D12_SRC CONFIGURE_DEPENDS "NVRHI/src/d3d12/*.cpp" "NVRHI/src/d3d12/*.h")
set_source_files_properties(${NVRHI_D3D12_SRC} PROPERTIES LANGUAGE CXX)

target_sources(NVRHI_D3D12 PRIVATE ${NVRHI_D3D12_SRC})

target_include_directories(NVRHI_D3D12 PUBLIC ${THIRDPARTY_DIR}/NVRHI/include)
# From premake
target_compile_definitions(NVRHI_D3D12 PUBLIC NVRHI_WITH_VALIDATION NVRHI_WITH_DX12 $<$<BOOL:WIN32>:NOMINMAX>)
set_common_target_options(NVRHI_D3D12)
set_target_properties(NVRHI_D3D12 PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# ShaderMake
add_library(ShaderMake STATIC
  ShaderMake/ShaderMake/src/argparse.c
  ShaderMake/ShaderMake/src/Compiler.cpp
  ShaderMake/ShaderMake/src/Context.cpp
  ShaderMake/ShaderMake/src/ShaderBlob.cpp
  ShaderMake/ShaderMake/include/ShaderMake/argparse.h
  ShaderMake/ShaderMake/include/ShaderMake/Compiler.h
  ShaderMake/ShaderMake/include/ShaderMake/Context.h
  ShaderMake/ShaderMake/include/ShaderMake/ShaderBlob.h
  ShaderMake/ShaderMake/include/ShaderMake/ShaderMake.h
  ShaderMake/ShaderMake/include/ShaderMake/Timer.h
)

# Mark headers as PUBLIC includes
target_include_directories(ShaderMake PUBLIC
  ShaderMake/ShaderMake/src
  ShaderMake/ShaderMake/include/ShaderMake
)

target_compile_definitions(ShaderMake PUBLIC SHADERMAKE_COLORS)
if(WIN32)
  target_compile_definitions(ShaderMake PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(ShaderMake PUBLIC d3dcompiler dxcompiler delayimp)
else()
  target_link_libraries(ShaderMake PUBLIC pthread)
endif()
set_common_target_options(ShaderMake)
set_target_properties(ShaderMake PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)

# JOLT
file(GLOB_RECURSE JOLT_SRC CONFIGURE_DEPENDS JOLT/Jolt/*.cpp)
add_library(JOLT STATIC ${JOLT_SRC})

target_include_directories(JOLT PUBLIC ${THIRDPARTY_DIR}/JOLT)

target_compile_definitions(JOLT PUBLIC
  JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
  JPH_DEBUG_RENDERER
  JPH_PROFILE_ENABLED
  JPH_OBJECT_STREAM
)
if(MSVC)
  target_compile_definitions(JOLT PUBLIC _WINDOWS WIN32 NOMINMAX)
endif()
set_common_target_options(JOLT)
set_target_properties(JOLT PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Place all third-party targets under a single 'Dependencies' solution folder in Visual Studio
set(_IGN_THIRDPARTY_TARGETS
  ZLIB GLFW STB SPDLOG IMGUI YAMLCPP ASSIMP NVRHI NVRHI_VULKAN NVRHI_D3D12 ShaderMake JOLT box2d
)
foreach(_t IN LISTS _IGN_THIRDPARTY_TARGETS)
  if(TARGET ${_t})
    set_property(TARGET ${_t} PROPERTY FOLDER "Dependencies")
  endif()
endforeach()
