cmake_minimum_required(VERSION 3.22)

project(ConvolutionReverbPlugin VERSION 1.0.0)

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "DataCogs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE DCog
    PLUGIN_CODE Cvrb
    FORMATS AU VST3 AAX Standalone
    AAX_CATEGORY AAX_ePlugInCategory_Reverb
    # PRODUCT_NAME is what DAWs display (and names the installed bundles).
    # PLUGIN_CODE / PLUGIN_MANUFACTURER_CODE stay fixed so existing sessions
    # keep resolving to this plugin across the rename.
    PRODUCT_NAME "DataCogs Reverb"
    COPY_PLUGIN_AFTER_BUILD ${COPY_PLUGIN_AFTER_BUILD}
)

juce_generate_juce_header(${PROJECT_NAME})

# PACE-sign the installed AAX bundle (shared recipe in common/cmake).
datacogs_sign_aax(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/IRGenerator.cpp
    source/IRTools.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        DataCogsUI
        juce::juce_audio_utils
        juce::juce_dsp
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
