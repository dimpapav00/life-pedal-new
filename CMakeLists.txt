cmake_minimum_required(VERSION 3.15)
project(LifePedalVST VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9 # Σταθερή έκδοση JUCE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LifePedalVST
    COMPANY_NAME "MyAudioLabs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MyaL
    PLUGIN_CODE Lfpd
    FORMATS VST3
    PRODUCT_NAME "LifePedalVST"
)

target_sources(LifePedalVST
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
)

target_compile_definitions(LifePedalVST
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Build_VST3=1
)

target_link_libraries(LifePedalVST
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
