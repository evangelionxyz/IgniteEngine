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