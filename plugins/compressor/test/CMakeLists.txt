cmake_minimum_required(VERSION 3.22)

project(CompressorTest)

set(CMAKE_CXX_STANDARD 20)

enable_testing()

add_executable(${PROJECT_NAME}
    source/main.cpp
    source/DspUnitTests.cpp
    source/ProcessorTests.cpp
    source/Helpers.cpp
    source/ImageProcessing.cpp
    source/Matchers.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../plugin/include
        ${JUCE_SOURCE_DIR}/modules
)

# These tests can use the Catch2-provided main
list(APPEND CMAKE_MODULE_PATH ${CATCH2_SOURCE_DIR}/extras)
find_package(Catch2 REQUIRED)
# include(${LIB_DIR}/catch2/extras/Catch.cmake)
include(CTest)
include(Catch)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CompressorPlugin
        juce::juce_audio_utils
        juce::juce_dsp
        Catch2::Catch2WithMain
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)


catch_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})