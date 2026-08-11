cmake_minimum_required(VERSION 3.22)

project(DataCogsPlugins)

set(CMAKE_CXX_STANDARD 20)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)

# download CPM.cmake
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
  ${LIB_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${LIB_DIR}/cmake/CPM.cmake)

# JUCE 8+ required: JUCE 7.x fails to configure against the macOS 15 SDK
# (juceaide hits the obsoleted CGWindowListCreateImage during *configure*).
# Downloaded ONCE here and shared by every plugin in the suite.
CPMAddPackage(
  NAME JUCE
  GITHUB_REPOSITORY juce-framework/JUCE
  GIT_TAG 8.0.14
  VERSION 8.0.14
  SOURCE_DIR ${LIB_DIR}/juce
)

CPMAddPackage(
  NAME CATCH2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.4.0
  SOURCE_DIR ${LIB_DIR}/catch2
)

# AAX (Pro Tools): JUCE 8 bundles the AAX SDK, so this is only for pinning
# a specific external SDK version if one is present.
set(AAX_SDK_PATH "$ENV{HOME}/SDKs/aax-sdk-2-9-0" CACHE PATH "Path to the Avid AAX SDK")
if(EXISTS ${AAX_SDK_PATH})
  juce_set_aax_sdk_path(${AAX_SDK_PATH})
endif()

# PACE signing for installed AAX bundles, shared by every plugin.
include(${CMAKE_CURRENT_SOURCE_DIR}/common/cmake/DataCogsAAX.cmake)

enable_testing()

# Each plugin can be switched off to configure a partial build, e.g.
#   cmake -S . -B build -DBUILD_PARAMETRIC_EQ=OFF -DBUILD_CONVOLUTION_REVERB=OFF
# (Single formats of a single plugin are always available as targets, e.g.
#   cmake --build build --target CompressorPlugin_AU)
option(BUILD_COMPRESSOR         "Build the Compressor plugin"         ON)
option(BUILD_PARAMETRIC_EQ      "Build the Parametric EQ plugin"      ON)
option(BUILD_CONVOLUTION_REVERB "Build the Convolution Reverb plugin" ON)

add_subdirectory(common)

if(BUILD_COMPRESSOR)
  add_subdirectory(plugins/compressor)
endif()
if(BUILD_PARAMETRIC_EQ)
  add_subdirectory(plugins/parametric-eq)
endif()
if(BUILD_CONVOLUTION_REVERB)
  add_subdirectory(plugins/convolution-reverb)
endif()
