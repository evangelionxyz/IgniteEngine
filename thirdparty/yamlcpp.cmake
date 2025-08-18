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