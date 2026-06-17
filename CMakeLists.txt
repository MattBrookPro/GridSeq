# -- GridSeq top-level build ---------------------------------------------------
#
# WHY this layout: the project is split into three independent CMake subtrees so
# that the *testable core* never depends on the GUI framework.
#
#   engine/  -> pure C++17, zero JUCE/UI dependency. This is what we unit-test.
#   tests/   -> Catch2 suite that links ONLY against the engine.
#   plugin/  -> the JUCE VST3/AU wrapper that consumes the engine.
#
# The plugin (and therefore the heavyweight JUCE download) is OFF by default, so
# `cmake -B build && cmake --build build` gives a fast engine+tests loop with no
# network fetch of JUCE. CI turns the plugin ON in a separate job.
# ------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.22)

project(GridSeq
    VERSION 0.1.0
    DESCRIPTION "A cross-platform step-sequencer instrument with a unit-tested engine"
    # C is enabled alongside CXX because JUCE ships C sources (and enables
    # Objective-C/C++ itself on Apple). Without C, generators that only enable
    # declared languages - Unix Makefiles on macOS/Linux - fail to configure the
    # plugin with "CMAKE_C_COMPILE_OBJECT not set".
    LANGUAGES C CXX)

# A single, explicit C++ standard for every target in the tree.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep build artefacts predictable across the matrix.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(GRIDSEQ_BUILD_TESTS  "Build the engine unit-test suite (Catch2)" ON)
option(GRIDSEQ_BUILD_PLUGIN "Build the JUCE VST3/AU plugin (downloads JUCE)" OFF)
option(GRIDSEQ_WERROR       "Treat engine warnings as errors (used in CI)" OFF)
option(GRIDSEQ_BUILD_TOOLS  "Build the offline render CLI used by QA tooling" ON)

# The engine is always built - it is the heart of the project and every other
# target depends on it.
add_subdirectory(engine)

if(GRIDSEQ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GRIDSEQ_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()

if(GRIDSEQ_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

message(STATUS "GridSeq: tests=${GRIDSEQ_BUILD_TESTS} plugin=${GRIDSEQ_BUILD_PLUGIN}")
