cmake_minimum_required(VERSION 3.23)

# IgniteScript: C# shared library
project(IgniteScript CSharp)

# Note: Requires CMake with CSharp support and a suitable generator (e.g., Visual Studio).
# Mirrors premake: kind SharedLib, framework 4.8, outputs to bin/lib/<config>

set(CMAKE_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8")

add_library(IgniteScript SHARED
  # Source discovery
)

# Collect C# sources
file(GLOB_RECURSE CSHARP_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/properties/*.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cs
)

set_property(TARGET IgniteScript PROPERTY VS_DOTNET_REFERENCES "System")
set_property(TARGET IgniteScript PROPERTY VS_GLOBAL_ROOTNAMESPACE IgniteScript)

# Assign sources
set_property(TARGET IgniteScript PROPERTY SOURCES ${CSHARP_SRC})

# Output dir similar to premake targetdir (bin/lib)
set_target_properties(IgniteScript PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/lib
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/lib
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/lib
)

# Optimize/symbols match
# CMake handles C# optimization flags via MSBuild defaults per config
