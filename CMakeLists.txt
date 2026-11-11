cmake_minimum_required(VERSION 3.22)
project(PepelVox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(PEPEL_BUILD_TOOLS "Build DSP test and screenshot tool" OFF)
option(PEPEL_ONLY_DSP_TEST "Only the pure C++ DSP test (no JUCE download)" OFF)
option(PEPEL_BUILD_PLUGIN "Build the plugin (VST3/AU/Standalone)" ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
set(FETCHCONTENT_QUIET OFF)

# --- тест DSP: чистый C++, JUCE не нужен
if(PEPEL_BUILD_TOOLS)
    add_executable(PepelDspTest Tests/DspTest.cpp)
    target_include_directories(PepelDspTest PRIVATE Source)
    if(MSVC)
        target_compile_options(PepelDspTest PRIVATE /utf-8)
    endif()
endif()

if(PEPEL_ONLY_DSP_TEST OR (NOT PEPEL_BUILD_PLUGIN AND NOT PEPEL_BUILD_TOOLS))
    return()
endif()

FetchContent_MakeAvailable(JUCE)

# --- шрифты (OFL), лицензии лежат рядом в Assets/
juce_add_binary_data(PepelAssets SOURCES
    Assets/VT323-Regular.ttf
    Assets/PressStart2P-Regular.ttf)

set(PEPEL_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

set(PEPEL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

if(PEPEL_BUILD_PLUGIN)
    if(APPLE)
        set(PEPEL_FORMATS VST3 AU Standalone)
    else()
        set(PEPEL_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(PepelVox
        COMPANY_NAME "PEPEL"
        PLUGIN_MANUFACTURER_CODE Pepl
        PLUGIN_CODE Pvox
        FORMATS ${PEPEL_FORMATS}
        PRODUCT_NAME "SUB-MIDI vox"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect
        BUNDLE_ID com.pepel.pepelvox)

    target_sources(PepelVox PRIVATE ${PEPEL_SOURCES})
    target_include_directories(PepelVox PRIVATE Source)
    target_compile_definitions(PepelVox PUBLIC ${PEPEL_DEFS})
    target_compile_options(PepelVox PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(PepelVox
        PRIVATE
            PepelAssets
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags)
endif()

# --- скриншоты: собираем напрямую из Source/*.cpp, без SharedCode
if(PEPEL_BUILD_TOOLS)
    add_executable(PepelScreenshot Tools/Screenshot.cpp ${PEPEL_SOURCES})
    target_include_directories(PepelScreenshot PRIVATE Source)
    target_compile_definitions(PepelScreenshot PRIVATE ${PEPEL_DEFS} JUCE_STANDALONE_APPLICATION=1)
    target_compile_options(PepelScreenshot PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(PepelScreenshot PRIVATE
        PepelAssets
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
