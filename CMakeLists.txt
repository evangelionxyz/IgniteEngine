cmake_minimum_required(VERSION 3.23)

project(IGN LANGUAGES C CXX)

# Enable solution folders in IDEs like Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Configurations (Debug, Release, Dist)
if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "" FORCE)
else()
	if(NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
	endif()
endif()

# Output layout: bin/<config> (under the chosen build tree)
foreach(dir RUNTIME LIBRARY ARCHIVE)
	set(CMAKE_${dir}_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
endforeach()


# Speed up MSVC builds similar to Premake's MultiProcessorCompile
if(MSVC)
	add_compile_options(/MP)
endif()

# Folders
set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)
set(RESOURCES_DIR  ${CMAKE_SOURCE_DIR}/resources)

# IncludeDir mapping from premake (used by targets below)
set(IncludeDir_GLFW            "${THIRDPARTY_DIR}/GLFW/include")
set(IncludeDir_BOX2D           "${THIRDPARTY_DIR}/BOX2D/include")
set(IncludeDir_ENTT            "${THIRDPARTY_DIR}/entt")
set(IncludeDir_GLM             "${THIRDPARTY_DIR}/GLM")
set(IncludeDir_IMGUI           "${THIRDPARTY_DIR}/IMGUI")
set(IncludeDir_IMGUIZMO        "${THIRDPARTY_DIR}/IMGUIZMO")
set(IncludeDir_NVRHI           "${THIRDPARTY_DIR}/NVRHI/include")
set(IncludeDir_SPDLOG          "${THIRDPARTY_DIR}/SPDLOG/include")
set(IncludeDir_STB             "${THIRDPARTY_DIR}/STB/include")
set(IncludeDir_VULKAN_SDK      "$ENV{VULKAN_SDK}/Include")
set(IncludeDir_NVRHI_VULKAN_HPP "${THIRDPARTY_DIR}/NVRHI/thirdparty/Vulkan-Headers/include")
set(IncludeDir_SHADERMAKE      "${THIRDPARTY_DIR}/ShaderMake/ShaderMake/include")
set(IncludeDir_ASSIMP          "${THIRDPARTY_DIR}/ASSIMP/include")
set(IncludeDir_YAMLCPP         "${THIRDPARTY_DIR}/YAML/include")
set(IncludeDir_FMOD            "${THIRDPARTY_DIR}/FMOD/include")
set(IncludeDir_JOLT            "${THIRDPARTY_DIR}/JOLT")
set(IncludeDir_MONO            "${THIRDPARTY_DIR}/MONO/include")
set(IncludeDir_FILEWATCHER     "${THIRDPARTY_DIR}/Filewatcher/include")
set(IncludeDir_ZLIB            "${THIRDPARTY_DIR}/ZLIB")
set(IncludeDir_TINYGLTF         "${THIRDPARTY_DIR}/TINYGLTF/include")
set(IncludeDir_MSDF_ATLAS_GEN  "${THIRDPARTY_DIR}/MSDFAtlasGen/msdf-atlas-gen")
set(IncludeDir_MSDFGEN         "${THIRDPARTY_DIR}/MSDFAtlasGen/msdfgen")
set(IncludeDir_FREETYPE        "${THIRDPARTY_DIR}/MSDFAtlasGen/msdfgen/freetype/include")

# Library mapping from premake (Windows specific)
if(WIN32)
	set(Library_winsock    ws2_32.lib)
	set(Library_winmm      winmm.lib)
	set(Library_winversion version.lib)
	set(Library_bcrypt     bcrypt.lib)

	# Vulkan SDK libraries; require VULKAN_SDK env var
	if(NOT DEFINED ENV{VULKAN_SDK})
		message(WARNING "VULKAN_SDK environment variable is not set. Vulkan/ShaderC libraries may fail to link.")
	endif()
	set(LibraryDir_VULKAN_SDK "$ENV{VULKAN_SDK}/Lib")
	set(Library_vulkan        "${LibraryDir_VULKAN_SDK}/vulkan-1.lib")

	# Mono & FMOD
	set(Library_mono "${THIRDPARTY_DIR}/MONO/lib/windows/libmono-static-sgen.lib")
	set(Library_FMOD "${THIRDPARTY_DIR}/FMOD/lib/windows/x64/fmod_vc.lib")

	# ShaderC / SPIRV-Cross / SPIRV-Tools (Debug/Release variants)
	set(Library_ShaderC_Debug                  "${LibraryDir_VULKAN_SDK}/shaderc_sharedd.lib")
	set(Library_SPIRV_Cross_Debug              "${LibraryDir_VULKAN_SDK}/spirv-cross-cored.lib")
	set(Library_SPIRV_Cross_GLSL_Debug         "${LibraryDir_VULKAN_SDK}/spirv-cross-glsld.lib")
	set(Library_SPIRV_Cross_HLSL_Debug         "${LibraryDir_VULKAN_SDK}/spirv-cross-hlsld.lib")
	set(Library_SPIRV_Cross_Reflect_Debug      "${LibraryDir_VULKAN_SDK}/spirv-cross-reflectd.lib")
	set(Library_SPIRV_Cross_Util_Debug         "${LibraryDir_VULKAN_SDK}/spirv-cross-utild.lib")
	set(Library_SPIRV_Tools_Debug              "${LibraryDir_VULKAN_SDK}/SPIRV-Toolsd.lib")

	set(Library_ShaderC_Release                "${LibraryDir_VULKAN_SDK}/shaderc_shared.lib")
	set(Library_SPIRV_Cross_Release            "${LibraryDir_VULKAN_SDK}/spirv-cross-core.lib")
	set(Library_SPIRV_Cross_GLSL_Release       "${LibraryDir_VULKAN_SDK}/spirv-cross-glsl.lib")
	set(Library_SPIRV_Cross_HLSL_Release       "${LibraryDir_VULKAN_SDK}/spirv-cross-hlsl.lib")
	set(Library_SPIRV_Cross_Reflect_Release    "${LibraryDir_VULKAN_SDK}/spirv-cross-reflect.lib")
	set(Library_SPIRV_Cross_Util_Release       "${LibraryDir_VULKAN_SDK}/spirv-cross-util.lib")
	set(Library_SPIRV_Tools_Release            "${LibraryDir_VULKAN_SDK}/SPIRV-Tools.lib")
endif()

# Subprojects
add_subdirectory(thirdparty)
add_subdirectory(engine)
add_subdirectory(editor)
add_subdirectory(runtime)

set(_IGN_RUNTIME_TARGETS IgniteEditor IgniteEditorV2 IgniteRuntime)
foreach(_t IN LISTS _IGN_RUNTIME_TARGETS)
  if(TARGET ${_t})
    set_property(TARGET ${_t} PROPERTY FOLDER "Runtime")
  endif()
endforeach()

set(_IGN_ENGINE_TARGETS IgniteEngine IgniteScript)
foreach(_t IN LISTS _IGN_ENGINE_TARGETS)
  if(TARGET ${_t})
    set_property(TARGET ${_t} PROPERTY FOLDER "Engine")
  endif()
endforeach()