cmake_minimum_required(VERSION 3.23)

project(IgniteEditorV2 CSharp)

set(CMAKE_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8")

file(GLOB_RECURSE CSHARP_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/properties/*.cs
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cs
)

file(GLOB_RECURSE XAML_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.xaml
)

add_executable(IgniteEditorV2
  ${CSHARP_SRC}
  ${XAML_SRC}
)

# Add WPF references
set_property(TARGET IgniteEditorV2 PROPERTY VS_DOTNET_REFERENCES 
  "System"
  "System.Xaml"
  "WindowsBase"
  "PresentationCore"
  "PresentationFramework"
  "Microsoft.CSharp"
)

set_property(TARGET IgniteEditorV2 PROPERTY VS_GLOBAL_ROOTNAMESPACE IgniteEditorV2)

# Configure XAML files
foreach(xaml_file ${XAML_SRC})
  set_property(SOURCE ${xaml_file} PROPERTY VS_XAML_TYPE "Page")
endforeach()

set_target_properties(IgniteEditorV2 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
  VS_GLOBAL_UseWPF "true"
  VS_GLOBAL_OutputType "WinExe"
)