cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
# Windows: static C++ runtime (no VC++ redistributable needed)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(CompAnatomyLab2 VERSION 0.2.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# local JUCE: -DJUCE_PATH=/path/to/JUCE (default: ~/Documents/jpkplugins/JUCE if present); otherwise fetched from GitHub
if(NOT JUCE_PATH AND EXISTS "$ENV{HOME}/Documents/jpkplugins/JUCE")
  set(JUCE_PATH "$ENV{HOME}/Documents/jpkplugins/JUCE")
endif()
if(JUCE_PATH)
  add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
  set(CA_FORMATS AU VST3 Standalone)
  if(NOT DEFINED CA_COPY)
    set(CA_COPY TRUE)
  endif()
else()
  set(CA_FORMATS VST3 Standalone)
  if(NOT DEFINED CA_COPY)
    set(CA_COPY FALSE)
  endif()
endif()

juce_add_plugin(CompAnatomyLab2
  COMPANY_NAME "JPK"
  PLUGIN_MANUFACTURER_CODE Jpkl
  PLUGIN_CODE Cmp2
  FORMATS ${CA_FORMATS}
  PRODUCT_NAME "Compressor Anatomy Lab 2"
  BUNDLE_ID com.jpk.companatomylab2
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx Dynamics
  COPY_PLUGIN_AFTER_BUILD ${CA_COPY})

target_sources(CompAnatomyLab2 PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
if(MSVC)
  target_compile_options(CompAnatomyLab2 PRIVATE /utf-8 /bigobj)   # Japanese strings, large translation units
endif()
target_compile_definitions(CompAnatomyLab2 PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(CompAnatomyLab2 PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
