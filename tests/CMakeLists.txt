# Catch2 is pulled in with FetchContent and pinned to an exact tag. WHY this
# rather than a vendored header: it demonstrates reproducible dependency
# management (the version is locked, the build is self-contained on a clean
# machine and in CI) without committing third-party source into the repo.
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(gridseq_tests
    test_smoke.cpp
    test_sampler_voice.cpp
    test_sequencer_timing.cpp
    test_swing.cpp
    test_state.cpp
    test_lockfree_fifo.cpp
    test_drumsynth.cpp
)

target_link_libraries(gridseq_tests PRIVATE gridseq_engine Catch2::Catch2WithMain)

# Catch2's CMake integration registers each TEST_CASE with CTest individually,
# so `ctest` reports per-case pass/fail - useful signal in CI logs.
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(gridseq_tests)
