# The JUCE plugin target. JUCE is pulled with FetchContent and pinned to an exact
# tag so the build is reproducible. This subtree is only configured when
# GRIDSEQ_BUILD_PLUGIN=ON, keeping the everyday engine+tests loop free of the
# (large) JUCE download.
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# IS_SYNTH + the AU/VST3 instrument categories make this register as a virtual
# instrument, not an effect. AU is built only on macOS (JUCE skips it elsewhere
# automatically).
juce_add_plugin(GridSeqPlugin
    PRODUCT_NAME            "GridSeq"
    COMPANY_NAME            "MattBrook"
    BUNDLE_ID               com.mattbrook.gridseq
    PLUGIN_MANUFACTURER_CODE Mbrk
    PLUGIN_CODE             Grsq
    FORMATS                 VST3 AU Standalone
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    VST3_CATEGORIES         Instrument
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(GridSeqPlugin PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp)

target_compile_features(GridSeqPlugin PRIVATE cxx_std_17)

# Trim JUCE features we don't use (smaller binary, fewer system deps in CI).
target_compile_definitions(GridSeqPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# The plugin links the very same engine library the unit tests link - the GUI is
# a thin shell over the tested core, which is the whole architectural point.
target_link_libraries(GridSeqPlugin
    PRIVATE
        gridseq_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
