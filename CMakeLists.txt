cmake_minimum_required(VERSION 3.15)

project(UhbikWrapper VERSION 0.0.1)

# Import JUCE using FetchContent
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(juce)

# Set plugin formats based on platform
if(APPLE)
    set(PLUGIN_FORMATS AU VST3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

# Create the plugin target
juce_add_plugin(UhbikWrapper
    COMPANY_NAME "MyCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE Uhbk
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Uhbik Wrapper"
)

target_sources(UhbikWrapper PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/EffectSlot.cpp
    Source/EffectSlot.h
    Source/PresetBrowser.cpp
    Source/PresetBrowser.h
)

target_compile_definitions(UhbikWrapper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(UhbikWrapper PRIVATE
    # Essential JUCE modules for a plugin that hosts other plugins
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
