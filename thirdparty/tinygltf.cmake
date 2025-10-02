add_library(TINYGLTF STATIC
  TINYGLTF/tinygltf.cpp
)
target_include_directories(TINYGLTF PUBLIC
  ${THIRDPARTY_DIR}/TINYGLTF/include
  ${THIRDPARTY_DIR}/STB/include
  ${THIRDPARTY_DIR}/JSON
)

set_common_target_options(YAMLCPP)
set_target_properties(TINYGLTF PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)