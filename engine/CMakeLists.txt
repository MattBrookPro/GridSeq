# The engine is a plain static library of portable C++17. No JUCE, no GUI, no
# audio-device code - that absence is the whole point: it is what makes the
# sequencing/timing logic unit-testable in isolation.
add_library(gridseq_engine STATIC
    src/SequencerEngine.cpp
    src/SamplerVoice.cpp
    src/Pattern.cpp
    src/StateCodec.cpp
)

target_include_directories(gridseq_engine PUBLIC include)
target_compile_features(gridseq_engine PUBLIC cxx_std_17)

# Position-independent code. WHY: this static library gets linked INTO the shared
# plugin binary (the VST3 .so / .dylib). On Linux/ELF, code pulled into a shared
# object must be compiled -fPIC or the link fails with a relocation error.
# macOS-clang and MSVC produce relocatable code by default, so this only bites on
# Linux - exactly the kind of thing the 3-OS CI matrix exists to catch.
set_target_properties(gridseq_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Treat the core as production code: warnings on, warnings-as-errors in CI.
if(MSVC)
    target_compile_options(gridseq_engine PRIVATE /W4)
else()
    target_compile_options(gridseq_engine PRIVATE -Wall -Wextra -Wpedantic)
    if(GRIDSEQ_WERROR)
        target_compile_options(gridseq_engine PRIVATE -Werror)
    endif()
endif()
