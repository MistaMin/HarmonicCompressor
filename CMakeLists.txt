cmake_minimum_required(VERSION 3.22)

# macOS target architecture / deployment target (arm64, macOS 11.0+)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture for macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

project(HarmonicCompressor VERSION 1.5.1)

if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

# Optional AAX SDK path. When set to a valid AAX SDK tree, the AAX format is
# added to the build; otherwise the project still configures cleanly without it.
set(AAX_SDK_PATH "" CACHE PATH "Path to the AAX SDK (optional)")

add_subdirectory(JUCE)

# GoodLookinUI (toolkit repo, checked out separately; this project holds no copy of it).
set(GOODLOOKINUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../HybridEQ/third_party/GoodLookinUI" CACHE PATH "GoodLookinUI toolkit location")
add_subdirectory("${GOODLOOKINUI_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/goodlookinui")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Designs/HarmonicCompressor.csv")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/HarmonicCompressor.csv" HC_DESIGN_TEXT)
configure_file(Designs/EmbeddedDesign.h.in generated/EmbeddedDesign.h @ONLY)

# Developer mode: builds a separate "Harmonic Compressor Dev" VST3 (own plugin code, so it can sit
# next to the release plugin in a DAW) with the GoodLookinUI inspector and look menus compiled in.
# Build it optimised (Release) so PluginDoctor measures the real DSP.
option(HARMONIC_DEV_MODE "Developer build: GoodLookinUI inspector, VST3 only, installed after build" OFF)

include(FetchContent)
option(HARMONIC_BUILD_CLAP "Build the CLAP plugin" ON)
if(HARMONIC_BUILD_CLAP AND NOT HARMONIC_DEV_MODE)
    set(HARMONIC_CLAP_EXT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../clap-juce-extensions" CACHE PATH "Use an existing clap-juce-extensions checkout")
    if(EXISTS "${HARMONIC_CLAP_EXT_SOURCE}/CMakeLists.txt")
        set(FETCHCONTENT_SOURCE_DIR_CLAP-JUCE-EXTENSIONS "${HARMONIC_CLAP_EXT_SOURCE}")
    endif()
    FetchContent_Declare(
        clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        55525c9858d4b25687be7759a5e0f70eccef218e
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# ---------------------------------------------------------------------------
# Plugin formats: VST3, AU and Standalone are always built. AAX is added only
# when AAX_SDK_PATH points at a valid AAX SDK (juce_set_aax_sdk_path must be
# called BEFORE juce_add_plugin, as required by JUCE's CMake API).
# ---------------------------------------------------------------------------
if(HARMONIC_DEV_MODE)
    set(HARMONIC_COMPRESSOR_FORMATS VST3)
    option(HARMONIC_DEV_STANDALONE "Also build a Standalone app in developer mode (for looking at the UI)" OFF)
    if(HARMONIC_DEV_STANDALONE)
        list(APPEND HARMONIC_COMPRESSOR_FORMATS Standalone)
    endif()
    set(HC_PRODUCT_NAME "Harmonic Compressor Dev")
    set(HC_PLUGIN_CODE Hcmd)
else()
    set(HARMONIC_COMPRESSOR_FORMATS VST3 AU Standalone LV2)
    set(HC_PRODUCT_NAME "Harmonic Compressor")
    set(HC_PLUGIN_CODE Hcmp)
endif()

if(AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}/Interfaces/ACF")
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND HARMONIC_COMPRESSOR_FORMATS AAX)
    message(STATUS "HarmonicCompressor: AAX SDK found at '${AAX_SDK_PATH}' - enabling AAX format")
else()
    message(STATUS "HarmonicCompressor: AAX SDK not found (AAX_SDK_PATH='${AAX_SDK_PATH}') - AAX format disabled")
endif()

juce_add_plugin(HarmonicCompressor
    FORMATS ${HARMONIC_COMPRESSOR_FORMATS}
    PLUGIN_MANUFACTURER_CODE OpGr
    PLUGIN_CODE ${HC_PLUGIN_CODE}
    PRODUCT_NAME "${HC_PRODUCT_NAME}"
    COPY_PLUGIN_AFTER_BUILD ${HARMONIC_DEV_MODE}
    COMPANY_NAME "OpenGrid"
    BUNDLE_ID com.opengrid.harmoniccompressor
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon-512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon-128.png"
    VST3_CATEGORIES "Fx Dynamics"
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY Dynamics
    LV2URI "urn:opengrid:harmoniccompressor")

juce_generate_juce_header(HarmonicCompressor)

target_sources(HarmonicCompressor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/CompressorCore.h
        Source/DSP/Adaa.h
        Source/DSP/Resampler.h
        Source/DSP/TransientDesigner.h
        Source/UI/Toolkit.h
        Source/UI/Panel.h
        Source/UI/Knob.h
        Source/UI/KeyButton.h
        Source/UI/FloatingModule.h
        Source/UI/MeterPanel.h
        Source/TransferCurveDisplay.h
        Source/TransferCurveDisplay.cpp)

target_include_directories(HarmonicCompressor PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(HarmonicCompressor
    PRIVATE
        goodlookinui_juce
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(HARMONIC_BUILD_CLAP AND NOT HARMONIC_DEV_MODE)
    clap_juce_extensions_plugin(TARGET HarmonicCompressor
        CLAP_ID "com.opengrid.harmoniccompressor"
        CLAP_FEATURES audio-effect compressor dynamics
        CLAP_MANUAL_URL ""
        CLAP_SUPPORT_URL "")
endif()

# This is a brand-new plugin with no VST2 release, so the VST3 parameter-ID
# migration warning (JUCE_VST3_CAN_REPLACE_VST2) does not apply.
target_compile_definitions(HarmonicCompressor PRIVATE
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
    GOODLOOKINUI_ENABLE_EDITOR=$<BOOL:${HARMONIC_DEV_MODE}>)

set_target_properties(HarmonicCompressor PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE)

# Headless click-mapping check for the linear module faders. Not part of the
# default plugin build; run with: cmake --build build --target SliderClickTest
add_executable(SliderClickTest EXCLUDE_FROM_ALL)
target_sources(SliderClickTest PRIVATE qa/2026-11-06-slider-click/slider_click_test.cpp)
target_include_directories(SliderClickTest PRIVATE
    $<TARGET_PROPERTY:HarmonicCompressor,INCLUDE_DIRECTORIES>)
target_link_libraries(SliderClickTest PRIVATE
    HarmonicCompressor
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
set_target_properties(SliderClickTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/SliderClickTest_artefacts/$<CONFIG>"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE
    EXCLUDE_FROM_ALL TRUE)

# Exercise the real editor and APVTS attachments, including recovery from maximum.
add_executable(EditorSliderTest EXCLUDE_FROM_ALL)
target_sources(EditorSliderTest PRIVATE qa/2026-11-06-slider-click/editor_slider_test.cpp)
target_include_directories(EditorSliderTest PRIVATE Source
    $<TARGET_PROPERTY:HarmonicCompressor,INCLUDE_DIRECTORIES>)
target_link_libraries(EditorSliderTest PRIVATE HarmonicCompressor
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
set_target_properties(EditorSliderTest PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED TRUE
    EXCLUDE_FROM_ALL TRUE)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME SliderClickTest COMMAND SliderClickTest)
    add_test(NAME EditorSliderTest COMMAND EditorSliderTest)
endif()
