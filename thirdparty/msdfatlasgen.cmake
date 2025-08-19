file(GLOB_RECURSE MSDF_ATLAS_GEN_SOURCE 
    MSDFAtlasGen/msdf-atlas-gen/*.cpp
    MSDFAtlasGen/msdf-atlas-gen/*.h
)
add_library(MSDF_ATLAS_GEN
    ${MSDF_ATLAS_GEN_SOURCE}
)

target_include_directories(MSDF_ATLAS_GEN PUBLIC
    MSDFAtlasGen/msdf-atlas-gen/
    MSDFAtlasGen/msdfgen/
    MSDFAtlasGen/msdfgen/include/
)

set_common_target_options(MSDF_ATLAS_GEN)
set_target_properties(MSDF_ATLAS_GEN PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
