-- VULKAN SDK
VULKAN_SDK_PATH = os.getenv("VULKAN_SDK")
if not VULKAN_SDK_PATH or VULKAN_SDK_PATH == "" then
    -- Try a default path if not set
    VULKAN_SDK_PATH = "C:/VulkanSDK/1.4.341.1"
end

-- Detect correct include folder casing
VULKAN_SDK_INCLUDE_PATH = VULKAN_SDK_PATH .. "/Include"
if not os.isdir(VULKAN_SDK_INCLUDE_PATH) then
    if os.isdir(VULKAN_SDK_PATH .. "/include") then
        VULKAN_SDK_INCLUDE_PATH = VULKAN_SDK_PATH .. "/include"
    end
end

-- Detect correct lib folder casing
VULKAN_SDK_LIB_PATH = VULKAN_SDK_PATH .. "/Lib"
if not os.isdir(VULKAN_SDK_LIB_PATH) then
    if os.isdir(VULKAN_SDK_PATH .. "/lib") then
        VULKAN_SDK_LIB_PATH = VULKAN_SDK_PATH .. "/lib"
    end
end

-- Detect correct bin folder casing
VULKAN_SDK_BIN_PATH = VULKAN_SDK_PATH .. "/Bin"
if not os.isdir(VULKAN_SDK_BIN_PATH) then
    if os.isdir(VULKAN_SDK_PATH .. "/bin") then
        VULKAN_SDK_BIN_PATH = VULKAN_SDK_PATH .. "/bin"
    end
end

print("VULKAN_SDK path: " .. tostring(VULKAN_SDK_PATH))
print("VULKAN_SDK include: " .. tostring(VULKAN_SDK_INCLUDE_PATH))

-- FBX SDK
FBX_SDK_PATH = os.getenv("FBX_SDK")
if not FBX_SDK_PATH then
    print("Error: FBX_SDK path environment variable is not set!")
end
print("FBX_SDK path: " .. tostring(FBX_SDK_PATH))

--includedirs
IncludeDir                          = {}
IncludeDir["ASSIM"]                 = "%{THIRDPARTY_DIR}/ASSIMP/include"
IncludeDir["GLFW"]                  = "%{THIRDPARTY_DIR}/GLFW/include"
IncludeDir["BOX2D"]                 = "%{THIRDPARTY_DIR}/BOX2D/include"
IncludeDir["ENTT"]                  = "%{THIRDPARTY_DIR}/entt/"
IncludeDir["GLM"]                   = "%{THIRDPARTY_DIR}/GLM/"
IncludeDir["IMGUI"]                 = "%{THIRDPARTY_DIR}/IMGUI/"
IncludeDir["IMGUIZMO"]              = "%{THIRDPARTY_DIR}/IMGUIZMO/"
IncludeDir["IMGUI_NODE"]            = "%{THIRDPARTY_DIR}/imgui_node_editor/"
IncludeDir["NVRHI"]                 = "%{THIRDPARTY_DIR}/NVRHI/include"
IncludeDir["SPDLOG"]                = "%{THIRDPARTY_DIR}/SPDLOG/include"
IncludeDir["STB"]                   = "%{THIRDPARTY_DIR}/STB/include"
IncludeDir["YAMLCPP"]               = "%{THIRDPARTY_DIR}/YAML/include"
IncludeDir["FMOD"]                  = "%{THIRDPARTY_DIR}/FMOD/include"
IncludeDir["JOLT"]                  = "%{THIRDPARTY_DIR}/JOLT"
IncludeDir["MONO"]                  = "%{THIRDPARTY_DIR}/Mono/include"
IncludeDir["ZLIB"]                  = "%{THIRDPARTY_DIR}/ZLIB"
IncludeDir["SDL3"]                  = "%{THIRDPARTY_DIR}/SDL3/include"
IncludeDir["JSON"]                  = "%{THIRDPARTY_DIR}/JSON"
IncludeDir["TINYGLTF"]              = "%{THIRDPARTY_DIR}/TINYGLTF/include"
IncludeDir["FILEWATCHER"]           = "%{THIRDPARTY_DIR}/Filewatcher/include"
IncludeDir["NVRHI_VULKAN_HEADERS"]  = "%{THIRDPARTY_DIR}/Vulkan-Headers/include"
IncludeDir["NVRHI_DIRECTX_HEADERS"] = "%{THIRDPARTY_DIR}/DirectX-Headers/include"
IncludeDir["MochiSharpNative"]      = "%{THIRDPARTY_DIR}/MochiSharp/MochiSharp.Native/Source"
IncludeDir["VULKAN_SDK"]            = VULKAN_SDK_INCLUDE_PATH
IncludeDir["FBX_SDK"]               = "%{FBX_SDK_PATH}/include"
IncludeDir["Hostfxr"]               = "%{THIRDPARTY_DIR}/MochiSharp/NetCore/include"
IncludeDir["MSDFATLASGEN"]          = "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdf-atlas-gen"
IncludeDir["MSDFGEN"]               = "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen"
IncludeDir["FREETYPE"]              = "%{THIRDPARTY_DIR}/MSDF-ATLAS-GEN/msdfgen/freetype/include"
IncludeDir["OPENEXR"]               = "%{THIRDPARTY_DIR}/OpenEXR/include/OpenEXR"
IncludeDir["IMATH"]                 = "%{THIRDPARTY_DIR}/Imath/include/Imath"
IncludeDir["TRACY"]                 = "%{THIRDPARTY_DIR}/tracy/public"
IncludeDir["NUKLEAR"]               = "%{THIRDPARTY_DIR}/Nuklear"
IncludeDir["UmbraShaderCompiler"]   = "%{THIRDPARTY_DIR}/UmbraShaderCompiler/Include"
IncludeDir["gtest"]                 = "%{THIRDPARTY_DIR}/gtest/googletest/include"


-- =============== LINUX SYSTEM PATHS ===============
-- These are used inside filter "system:linux" blocks in project files.
IncludeDir["OPENEXR_LINUX"]          = "/usr/include/OpenEXR"
IncludeDir["IMATH_LINUX"]            = "/usr/include/Imath"
IncludeDir["SDL3_LINUX"]             = "/usr/include/SDL3"

--library dirs
LibraryDir                           = {}
LibraryDir["VULKAN_SDK"]             = VULKAN_SDK_LIB_PATH
LibraryDir["VULKAN_SDK_BIN"]         = VULKAN_SDK_BIN_PATH
LibraryDir["FBX_SDK"]                = "%{FBX_SDK_PATH}/lib"

-- =============== WINDOWS ONLY ===============
Library                              = {}
Library["winsock"]                   = "ws2_32.lib"
Library["winmm"]                     = "winmm.lib"
Library["winversion"]                = "version.lib"
Library["bcrypt"]                    = "bcrypt.lib"
Library["vulkan"]                    = "%{LibraryDir.VULKAN_SDK}/vulkan-1.lib"
Library["mono"]                      = "%{THIRDPARTY_DIR}/Mono/lib/windows/libmono-static-sgen.lib"
Library["Hostfxr"]                   = "%{THIRDPARTY_DIR}/MochiSharp/ThirdParty/dotnet/host/fxr/9.0.11/x64/nethost.lib"

Library["ASSIMP"]                    = "%{THIRDPARTY_DIR}/ASSIMP/lib/win32/assimp-vc143-mt.lib"
Library["FMOD"]                      = "%{THIRDPARTY_DIR}/FMOD/lib/windows/x64/fmod_vc.lib"
Library["SDL3"]                      = "%{THIRDPARTY_DIR}/SDL3/lib/windows/x64/SDL3.lib"

Library["Iex"]                       = "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/Iex-3_4.lib"
Library["OpenEXR"]                   = "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXR-3_4.lib"
Library["OpenEXRCore"]               = "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRCore-3_4.lib"
Library["OpenEXRUtil"]               = "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/OpenEXRUtil-3_4.lib"
Library["IlmThread"]                 = "%{THIRDPARTY_DIR}/OpenEXR/lib/win32/IlmThread-3_4.lib"
Library["Imath"]                     = "%{THIRDPARTY_DIR}/Imath/lib/win32/Imath-3_2.lib"

Library["ShaderC_Debug"]             = "%{LibraryDir.VULKAN_SDK}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Core_Debug"]    = "%{LibraryDir.VULKAN_SDK}/spirv-cross-cored.lib"
Library["SPIRV_Cross_CPP_Debug"]     = "%{LibraryDir.VULKAN_SDK}/spirv-cross-cppd.lib"
Library["SPIRV_Cross_MSL_Debug"]     = "%{LibraryDir.VULKAN_SDK}/spirv-cross-msld.lib"
Library["SPIRV_Cross_GLSL_Debug"]    = "%{LibraryDir.VULKAN_SDK}/spirv-cross-glsld.lib"
Library["SPIRV_Cross_HLSL_Debug"]    = "%{LibraryDir.VULKAN_SDK}/spirv-cross-hlsld.lib"
Library["SPIRV_Cross_Reflect_Debug"] = "%{LibraryDir.VULKAN_SDK}/spirv-cross-reflectd.lib"
Library["SPIRV_Cross_Util_Debug"]    = "%{LibraryDir.VULKAN_SDK}/spirv-cross-utild.lib"
Library["SPIRV_Tools_Debug"]         = "%{LibraryDir.VULKAN_SDK}/SPIRV-Toolsd.lib"

Library["ShaderC"]                   = "%{LibraryDir.VULKAN_SDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Core"]          = "%{LibraryDir.VULKAN_SDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_CPP"]           = "%{LibraryDir.VULKAN_SDK}/spirv-cross-cpp.lib"
Library["SPIRV_Cross_MSL"]           = "%{LibraryDir.VULKAN_SDK}/spirv-cross-msl.lib"
Library["SPIRV_Cross_GLSL"]          = "%{LibraryDir.VULKAN_SDK}/spirv-cross-glsl.lib"
Library["SPIRV_Cross_HLSL"]          = "%{LibraryDir.VULKAN_SDK}/spirv-cross-hlsl.lib"
Library["SPIRV_Cross_Reflect"]       = "%{LibraryDir.VULKAN_SDK}/spirv-cross-reflect.lib"
Library["SPIRV_Cross_Util"]          = "%{LibraryDir.VULKAN_SDK}/spirv-cross-util.lib"
Library["SPIRV_Tools"]               = "%{LibraryDir.VULKAN_SDK}/SPIRV-Tools.lib"

-- Vulkan C API
Library["SPIRV_Cross_C_Debug"]       = "%{LibraryDir.VULKAN_SDK}/spirv-cross-cd.lib"
Library["SPIRV_Cross_C"]             = "%{LibraryDir.VULKAN_SDK}/spirv-cross-c.lib"

-- AUTODESK FBX
Library["FBX_ALEMBIC_DEBUG"]         = "%{LibraryDir.FBX_SDK}/x64/debug/alembic.lib"
Library["FBX_SDK_DEBUG"]             = "%{LibraryDir.FBX_SDK}/x64/debug/libfbxsdk-md.lib"
Library["FBX_XML_DEBUG"]             = "%{LibraryDir.FBX_SDK}/x64/debug/libxml2.lib"

Library["FBX_ALEMBIC"]               = "%{LibraryDir.FBX_SDK}/x64/release/alembic.lib"
Library["FBX_SDK"]                   = "%{LibraryDir.FBX_SDK}/x64/release/libfbxsdk-md.lib"
Library["FBX_XML"]                   = "%{LibraryDir.FBX_SDK}/x64/release/libxml2.lib"


-- =============== LINUX ONLY ===============
-- FMOD bundled .so files (already in the repo at thirdparty/FMOD/lib/linux)
LibraryDir["FMOD_LINUX"]            = "%{THIRDPARTY_DIR}/FMOD/lib/linux/x64"

-- FBX SDK Linux (GCC tarball layout: lib/gcc/x64/{debug,release}/libfbxsdk.so)
-- FBX_SDK_PATH is set via FBX_SDK env var, same as Windows.
LibraryDir["FBX_SDK_LINUX_DEBUG"]   = "%{FBX_SDK_PATH}/lib/gcc/x64/debug"
LibraryDir["FBX_SDK_LINUX_RELEASE"] = "%{FBX_SDK_PATH}/lib/gcc/x64/release"

-- include lua files
group "Third Party"
include "box2d.lua"
include "spdlog.lua"
include "jolt.lua"
include "imgui.lua"
include "stb.lua"
include "yaml-cpp.lua"
include "tinygltf.lua"
include "zlib.lua"
include "msdf-atlas-gen.lua"
include "msdfgen.lua"
include "freetype.lua"
include "tracy.lua"
include "gtest.lua"
include "umbrashadercompiler.lua"
include "nvrhi.lua"
group ""
