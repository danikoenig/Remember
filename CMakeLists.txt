cmake_minimum_required(VERSION 3.22)

project(Remember VERSION 1.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: lokaler Pfad (-DJUCE_PATH=...) oder automatischer Download
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# CLAP (Bitwig u.a.) ueber clap-juce-extensions
include(FetchContent)
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
    GIT_SUBMODULES_RECURSE TRUE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(clap-juce-extensions)

if(APPLE)
    set(REMEMBER_FORMATS VST3 AU)
else()
    set(REMEMBER_FORMATS VST3)
endif()

juce_add_plugin(Remember
    COMPANY_NAME "Dani Koenig"
    BUNDLE_ID "ch.danikoenig.remember"
    PLUGIN_MANUFACTURER_CODE Dkng
    PLUGIN_CODE Rmbr
    FORMATS ${REMEMBER_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "Remember"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools)

target_sources(Remember PRIVATE
    Source/Audition.cpp
    Source/Clip.cpp
    Source/ClipView.cpp
    Source/Engine.cpp
    Source/Export.cpp
    Source/HistoryView.cpp
    Source/Look.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Remember PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Remember
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET Remember
    CLAP_ID "ch.danikoenig.remember"
    CLAP_FEATURES audio-effect utility stereo mono)

# Engine-Test ohne JUCE (cmake --build build --target RememberEngineTest)
add_executable(RememberEngineTest EXCLUDE_FROM_ALL tests/EngineTest.cpp Source/Engine.cpp)
add_executable(RememberClipTest EXCLUDE_FROM_ALL tests/ClipTest.cpp Source/Clip.cpp Source/Audition.cpp)

# Entwickler-Werkzeug: rendert die Oberflaeche mit Testdaten in ein PNG (cmake -DREMEMBER_SNAPSHOT_TOOL=ON)
option(REMEMBER_SNAPSHOT_TOOL "Build the UI snapshot tool" OFF)
if(REMEMBER_SNAPSHOT_TOOL)
    juce_add_console_app(RememberSnapshot PRODUCT_NAME "RememberSnapshot")
    target_sources(RememberSnapshot PRIVATE
        tools/Snapshot.cpp Source/Audition.cpp Source/Clip.cpp Source/ClipView.cpp Source/Engine.cpp
        Source/Export.cpp Source/HistoryView.cpp
        Source/Look.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(RememberSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_VersionString="0.1.0" JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(RememberSnapshot PRIVATE juce::juce_audio_utils juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags)
endif()
