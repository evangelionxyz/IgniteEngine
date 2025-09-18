cmake_minimum_required(VERSION 3.23)

project(IgniteEngineProj LANGUAGES C CXX)

add_library(IgniteEngine STATIC)

# Collect engine sources
file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ignite/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ignite/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ignite/*.h
)

# utf-8 build options
target_compile_options(IgniteEngine PRIVATE 
    $<$<C_COMPILER_ID:MSVC>:/utf-8> 
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
target_sources(IgniteEngine PRIVATE ${ENGINE_SRC})

# Do not compile shader files
file(GLOB_RECURSE HLSL_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/resources/shaders/*.hlsl)
file(GLOB_RECURSE GLSL_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/resources/shaders/*.glsl)
set_source_files_properties(${HLSL_FILES} ${GLSL_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Include directories
target_include_directories(IgniteEngine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${IncludeDir_GLFW}
  ${IncludeDir_BOX2D}
  ${IncludeDir_ENTT}
  ${IncludeDir_FMOD}
  ${IncludeDir_GLM}
  ${IncludeDir_JOLT}
  ${IncludeDir_IMGUI}
  ${IncludeDir_IMGUIZMO}
  ${IncludeDir_SPDLOG}
  ${IncludeDir_MONO}
  ${IncludeDir_NVRHI}
  ${IncludeDir_STB}
  ${IncludeDir_NVRHI_VULKAN_HPP}
  ${IncludeDir_VULKAN_SDK}
  ${IncludeDir_FILEWATCHER}
  ${IncludeDir_SHADERMAKE}
  ${IncludeDir_ZLIB}
  ${IncludeDir_ASSIMP}
  ${IncludeDir_YAMLCPP}
  ${IncludeDir_MSDF_ATLAS_GEN}
  ${IncludeDir_MSDFGEN}
  ${IncludeDir_FREETYPE}
)

# Public defines
target_compile_definitions(IgniteEngine PUBLIC
  SHADERMAKE_COLORS
  YAML_CPP_STATIC_DEFINE
  JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
  JPH_DEBUG_RENDERER
  JPH_PROFILE_ENABLED
  JPH_OBJECT_STREAM
)

# Link thirdparty static libs
target_link_libraries(IgniteEngine PUBLIC
  IMGUI box2d GLFW STB JOLT SPDLOG ShaderMake
  ASSIMP NVRHI NVRHI_VULKAN ZLIB YAMLCPP
  MSDF_ATLAS_GEN MSDFGEN TINYGLTF
)

if(WIN32)
  # Windows specific links and defines
  target_link_libraries(IgniteEngine PUBLIC
    d3d12 dxgi d3dcompiler dxguid
    NVRHI_D3D12 NVRHI_VULKAN
    ${Library_winmm} ${Library_winsock} ${Library_winversion} ${Library_bcrypt}
    ${Library_vulkan} ${Library_mono} ${Library_FMOD}
  )
  target_compile_definitions(IgniteEngine PUBLIC
    PLATFORM_WINDOWS
    GLFW_EXPOSE_NATIVE_WIN32
    NOMINMAX
    IGNITE_WITH_DX12
    IGNITE_WITH_VULKAN
    _CRT_SECURE_NO_WARNINGS
    _WINDOWS
    WIN32
  )

  # Copy FMOD DLL, Resources folder, lib folder
  add_custom_command(TARGET IgniteEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${THIRDPARTY_DIR}/FMOD/lib/windows/x64/fmod.dll" "$<TARGET_FILE_DIR:IgniteEngine>"
    
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
      "${CMAKE_SOURCE_DIR}/resources" "$<TARGET_FILE_DIR:IgniteEngine>/resources"

    COMMAND ${CMAKE_COMMAND} -E copy_directory 
      "${CMAKE_SOURCE_DIR}/editor/lib" "$<TARGET_FILE_DIR:IgniteEngine>/lib"
  )

  # ShaderC/Spirv selection per config
  target_link_libraries(IgniteEngine PUBLIC
    $<$<CONFIG:Debug>:${Library_ShaderC_Debug} ${Library_SPIRV_Cross_Debug} ${Library_SPIRV_Cross_GLSL_Debug} ${Library_SPIRV_Cross_HLSL_Debug} ${Library_SPIRV_Cross_Reflect_Debug} ${Library_SPIRV_Cross_Util_Debug} ${Library_SPIRV_Tools_Debug}>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Dist>>:${Library_ShaderC_Release} ${Library_SPIRV_Cross_Release} ${Library_SPIRV_Cross_GLSL_Release} ${Library_SPIRV_Cross_HLSL_Release} ${Library_SPIRV_Cross_Reflect_Release} ${Library_SPIRV_Cross_Util_Release} ${Library_SPIRV_Tools_Release}>
  )
else()
  # Linux defaults (mirrored from premake)
  target_compile_definitions(IgniteEngine PUBLIC PLATFORM_LINUX _GLFW_X11)
  target_link_libraries(IgniteEngine PUBLIC vulkan shaderc_shared spirv-cross-core spirv-cross-glsl pthread dl m rt glib-2.0)
endif()

# C++ standard
set_target_properties(IgniteEngine PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)
