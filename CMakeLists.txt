cmake_minimum_required(VERSION 3.22)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/framework/cmake/GaDefaults.cmake)
    message(FATAL_ERROR "framework/ is empty. Get the GA framework with: git submodule update --init")
endif()

# Universal build (Apple Silicon + Intel), macOS 11 or newer: GA defaults, before project().
include(framework/cmake/GaDefaults.cmake)

project(FasePhucker VERSION 0.1.0)

# JUCE and ga_add_plugin(). JUCE (the version in framework/cmake/juce-version.txt) is found next to this
# repo (../JUCE), otherwise at: cmake -B build -DJUCE_DIR=/path/to/JUCE
# No post-build copy: see the README for installing.
include(framework/cmake/GaPlugin.cmake)

file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Source/dsp/*.cpp)

ga_add_plugin(FasePhucker
    PRODUCT_NAME "Fase Phucker"
    PLUGIN_CODE Fph1
    BUNDLE_ID com.ga.fasephucker
    SOURCES ${SOURCES})

# Screenshot tool: renders the real interface into PNGs. Built in its own folder, so it never ends up
# in the plug-ins: cmake -B build-tools -DFPH_SCREENSHOTS=ON && cmake --build build-tools --target FasePhuckerScreenshots
ga_add_tool(FasePhucker FasePhuckerScreenshots
    OPTION FPH_SCREENSHOTS DESCRIPTION "Build the tool that renders screenshots of the interface"
    SOURCES tools/Screenshots.cpp MAIN tools/ScreenshotsMain.cpp)

# Parameter tool: the reference list of every parameter (tests/reference/parameters.txt) and the
# parameter tests. Built in its own folder too: see tools/check_parameters.sh
ga_add_tool(FasePhucker FasePhuckerParams
    OPTION FPH_PARAMS DESCRIPTION "Build the parameter list and tests"
    SOURCES tools/Parameters.cpp MAIN tools/ParametersMain.cpp)

# The GA control gallery (framework/docs/controls): see framework/cmake/GaPlugin.cmake
ga_add_gallery(FasePhucker)

# DSP test tool: measured checks of the engine and renders through the plug-in compared with
# tests/reference/renders. Built in its own folder too: see tools/check_dsp.sh
ga_add_tool(FasePhucker FasePhuckerRender
    OPTION FPH_RENDER DESCRIPTION "Build the DSP checks and renders"
    SOURCES tools/Render.cpp MAIN tools/RenderMain.cpp)
