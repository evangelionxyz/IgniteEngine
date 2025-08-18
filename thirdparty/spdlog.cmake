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