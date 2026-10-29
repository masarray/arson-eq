cmake_minimum_required(VERSION 3.24)
project(ArSonEQ VERSION 0.1.0 LANGUAGES CXX)

option(ARSONEQ_BUILD_PLUGIN "Build VST3 plugin using JUCE" OFF)
option(ARSONEQ_BUILD_TESTS "Build standalone DSP unit tests" ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(arson_eq_dsp INTERFACE)
target_include_directories(arson_eq_dsp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(arson_eq_dsp INTERFACE cxx_std_17)

if(ARSONEQ_BUILD_TESTS)
    enable_testing()
    add_executable(arson_eq_core_tests tests/eq_core_tests.cpp)
    target_link_libraries(arson_eq_core_tests PRIVATE arson_eq_dsp)
    if(MSVC)
        target_compile_options(arson_eq_core_tests PRIVATE /W4)
    else()
        target_compile_options(arson_eq_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME arson_eq_core COMMAND arson_eq_core_tests)
endif()

if(ARSONEQ_BUILD_PLUGIN)
    # Local JUCE checkout for offline builds; otherwise fetch a pinned release.
    if(DEFINED JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
    else()
        include(FetchContent)
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 9.0.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(juce)
    endif()

    juce_add_plugin(ArSonEQ
        COMPANY_NAME "ArSon Audio"
        BUNDLE_ID "com.arsonaudio.arsoneq"
        PLUGIN_MANUFACTURER_CODE ArsN
        PLUGIN_CODE ArEQ
        FORMATS VST3
        VST3_CATEGORIES Fx EQ
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PRODUCT_NAME "ArSonEQ")
    target_sources(ArSonEQ PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_link_libraries(ArSonEQ PRIVATE arson_eq_dsp
        juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags)
    target_compile_definitions(ArSonEQ PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
endif()
