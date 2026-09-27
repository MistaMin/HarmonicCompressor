cmake_minimum_required(VERSION 3.22)

# macOS target architecture / deployment target (arm64, macOS 11.0+)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture for macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

project(HarmonicCompressor VERSION 1.4.0)

# Optional AAX SDK path. When set to a valid AAX SDK tree, the AAX format is
# added to the build; otherwise the project still configures cleanly without it.
set(AAX_SDK_PATH "" CACHE PATH "Path to the AAX SDK (optional)")

add_subdirectory(JUCE)

# ---------------------------------------------------------------------------
# Plugin formats: VST3, AU and Standalone are always built. AAX is added only
# when AAX_SDK_PATH points at a valid AAX SDK (juce_set_aax_sdk_path must be
# called BEFORE juce_add_plugin, as required by JUCE's CMake API).
# ---------------------------------------------------------------------------
set(HARMONIC_COMPRESSOR_FORMATS VST3 AU Standalone)

if(AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}/Interfaces/ACF")
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND HARMONIC_COMPRESSOR_FORMATS AAX)
    message(STATUS "HarmonicCompressor: AAX SDK found at '${AAX_SDK_PATH}' - enabling AAX format")
else()
    message(STATUS "HarmonicCompressor: AAX SDK not found (AAX_SDK_PATH='${AAX_SDK_PATH}') - AAX format disabled")
endif()

juce_add_plugin(HarmonicCompressor
    FORMATS ${HARMONIC_COMPRESSOR_FORMATS}
    PLUGIN_MANUFACTURER_CODE Mncp
    PLUGIN_CODE Hcmp
    PRODUCT_NAME "Harmonic Compressor"
    COMPANY_NAME "MinComp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES "Fx Dynamics"
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY Dynamics)

juce_generate_juce_header(HarmonicCompressor)

target_sources(HarmonicCompressor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/CompressorCore.h
        Source/LookAndFeelCustom.h
        Source/LookAndFeelCustom.cpp
        Source/VuMeterDisplay.h
        Source/VuMeterDisplay.cpp
        Source/TransferCurveDisplay.h
        Source/TransferCurveDisplay.cpp)

target_link_libraries(HarmonicCompressor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# This is a brand-new plugin with no VST2 release, so the VST3 parameter-ID
# migration warning (JUCE_VST3_CAN_REPLACE_VST2) does not apply.
target_compile_definitions(HarmonicCompressor PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

set_target_properties(HarmonicCompressor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED TRUE)
