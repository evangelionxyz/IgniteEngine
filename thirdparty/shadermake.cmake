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