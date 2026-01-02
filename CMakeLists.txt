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

# Import CLAP JUCE extensions for CLAP plugin format support
# This also includes the CLAP SDK and clap-helpers which we use for hosting
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(clap-juce-extensions)

# Set plugin formats based on platform
if(APPLE)
    set(PLUGIN_FORMATS AU VST3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

# Create the plugin target
# Note: PRODUCT_NAME has no space to avoid Bitwig/macOS compatibility issues
juce_add_plugin(UhbikWrapper
    COMPANY_NAME "Inclusive Audio"
    BUNDLE_ID "com.inclusiveaudio.uhbikwrapper"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE IncA
    PLUGIN_CODE UhWr
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "UhbikWrapper"
)

# CLAP format export - re-enabled for testing
# Note: May have sidechain bus compatibility issues in some hosts
clap_juce_extensions_plugin(
    TARGET UhbikWrapper
    CLAP_ID "com.inclusiveaudio.uhbikwrapper"
    CLAP_FEATURES audio-effect utility
    CLAP_MANUAL_URL "https://github.com/davelarsen58/UhbikWrapper"
    CLAP_SUPPORT_URL "https://github.com/davelarsen58/UhbikWrapper/issues"
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
    Source/CLAPPluginHost.cpp
    Source/CLAPPluginHost.h
)

target_compile_definitions(UhbikWrapper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
)

# Find X11 for CLAP GUI hosting on Linux
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif()

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
    # CLAP hosting support (from clap-juce-extensions bundled libraries)
    clap
    clap-helpers
    # X11 for CLAP plugin GUI embedding on Linux
    $<$<PLATFORM_ID:Linux>:${X11_LIBRARIES}>
)

# Add CLAP include directories (from clap-juce-extensions)
target_include_directories(UhbikWrapper PRIVATE
    ${clap-juce-extensions_SOURCE_DIR}/clap-libs/clap/include
    ${clap-juce-extensions_SOURCE_DIR}/clap-libs/clap-helpers/include
)
