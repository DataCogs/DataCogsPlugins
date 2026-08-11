cmake_minimum_required(VERSION 3.22)

project(ParametricEQTest)

set(CMAKE_CXX_STANDARD 20)

enable_testing()

add_executable(${PROJECT_NAME}
    source/main.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../plugin/include
        ${JUCE_SOURCE_DIR}/modules
)

list(APPEND CMAKE_MODULE_PATH ${CATCH2_SOURCE_DIR}/extras)
find_package(Catch2 REQUIRED)
include(CTest)
include(Catch)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ParametricEQPlugin
        juce::juce_audio_utils
        juce::juce_dsp
        Catch2::Catch2WithMain
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

catch_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
