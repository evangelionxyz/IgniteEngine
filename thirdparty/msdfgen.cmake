file(GLOB_RECURSE MSDFGEN_SOURCE
    MSDFAtlasGen/msdfgen/core/*.cpp
    MSDFAtlasGen/msdfgen/core/*.h
    MSDFAtlasGen/msdfgen/ext/*.cpp
    MSDFAtlasGen/msdfgen/ext/*.h
    MSDFAtlasGen/msdfgen/lib/*.cpp
)

add_library(MSDFGEN STATIC
    ${MSDFGEN_SOURCE}
)

target_include_directories(MSDFGEN PRIVATE
    MSDFAtlasGen/msdfgen/
    MSDFAtlasGen/msdfgen/include
    MSDFAtlasGen/msdfgen/freetype/include
)

target_compile_definitions(MSDFGEN PUBLIC
    MSDF_USE_CPP11
)

set_common_target_options(MSDF_ATLAS_GEN)
set_target_properties(MSDF_ATLAS_GEN PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
