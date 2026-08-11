cmake_minimum_required(VERSION 3.22)

project(CompressorPlugin VERSION 0.1.0)

# find_package(JUCE CONFIG REQUIRED)

# Installing into the system plugin folders needs local admin rights; CI
# turns this off (-DCOPY_PLUGIN_AFTER_BUILD=OFF) and ships zipped artefacts.
option(COPY_PLUGIN_AFTER_BUILD "Copy plugins into the system plugin folders after each build" ON)

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "DataCogs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE DCog
    PLUGIN_CODE MBC1
    FORMATS AU VST3 AAX Standalone
    AAX_CATEGORY AAX_ePlugInCategory_Dynamics
    # PRODUCT_NAME is what DAWs display (and names the installed bundles).
    # PLUGIN_CODE / PLUGIN_MANUFACTURER_CODE stay fixed so existing sessions
    # keep resolving to this plugin across the rename.
    PRODUCT_NAME "DataCogs Compressor"
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
)

# https://forum.juce.com/t/how-to-link-install-library-to-target-plugin-component-using-cmake/46902
# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. This should be included with `#include <JuceHeader.h>`. The include path for
# this header will be automatically added to the target. The main function of the JuceHeader is to
# include all your JUCE module headers; if you're happy to include module headers directly, you
# probably don't need to call this.

juce_generate_juce_header(${PROJECT_NAME})

# UI assets (DataCogs logo) baked into the binary as BinaryData.

# PACE-sign the installed AAX bundle (shared recipe in common/cmake).
datacogs_sign_aax(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/EnvelopeDetector.cpp
    source/Compressor.cpp
)


target_include_directories(${PROJECT_NAME} 
    PRIVATE
        include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        DataCogsUI
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
       
)

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# if (MSVC)
# target_compile_definitions(${PROJECT_NAME} 
#     PRIVATE
#         _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)

# CPack takes these paths as the locations where the plugins should be installed in the end
# install (TARGETS MYPLUGIN_VST3 LIBRARY DESTINATION /Library/Audio/Plug-Ins/VST3       COMPONENT VST3)
# install (TARGETS MYPLUGIN_AU   LIBRARY DESTINATION /Library/Audio/Plug-Ins/Components COMPONENT AU)

# # This makes sure that only the two components and nothing else is part of our installer
# SET (CPACK_COMPONENTS_ALL VST3 AU)

# # We want to use the native Apple GUI installer
# SET (CPACK_GENERATOR productbuild)

# if(UNIX AND NOT APPLE)
# target_link_libraries(${PROJECT_NAME} dl pthread m)
# endif()

# if(APPLE)
# set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "@loader_path")
# elseif(UNIX)
# set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "$ORIGIN")
# endif()

# install(TARGETS ${PROJECT_NAME} DESTINATION $ENV{HOME}/Library/Audio/Plug-Ins/Components/lib)
# install(
# DIRECTORY ${RTSE_DIR}/lib/
# DESTINATION $ENV{HOME}/Library/Audio/Plug-Ins/Components/lib
# FILES_MATCHING
#   PATTERN "*.dylib"
#   PATTERN "*.so*"
#   PATTERN "*.dll"
# )