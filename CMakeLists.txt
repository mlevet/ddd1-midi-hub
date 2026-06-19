cmake_minimum_required(VERSION 3.22)
project(DDD1MidiHub VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)

add_subdirectory(../DDD1BassTranslator/JUCE JUCE)

juce_add_plugin(DDD1MidiHub
    COMPANY_NAME "HomeMade"
    PLUGIN_MANUFACTURER_CODE HmMd
    PLUGIN_CODE Dmh1
    FORMATS Standalone
    PRODUCT_NAME "DDD-1 MIDI Hub"
    PLUGIN_DESCRIPTION "Full MIDI hub for Korg DDD-1 — 14 pads with per-pad routing"
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(DDD1MidiHub)

target_sources(DDD1MidiHub PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PatternBank.cpp
    Source/PatternSetBank.cpp
)

target_compile_definitions(DDD1MidiHub PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DDD1MidiHub PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
)
