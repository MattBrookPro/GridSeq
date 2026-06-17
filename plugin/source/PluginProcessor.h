#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "gridseq/LockFreeFifo.h"
#include "gridseq/SequencerEngine.h"

// -- GridSeqAudioProcessor -----------------------------------------------------
//
// The JUCE shell around the pure engine. Its jobs are exactly the "glue" the
// engine deliberately doesn't do:
//   * own the parameters (APVTS) and the GUI,
//   * deliver UI changes to the audio thread without locking,
//   * persist/restore state.
// Every musical decision still lives in the engine - this class never sequences
// anything itself.

namespace ids {
// Parameter IDs kept in one place so the editor's attachments and the processor
// can't drift apart.
constexpr auto run    = "run";
constexpr auto tempo  = "tempo";
constexpr auto swing  = "swing";
constexpr auto master = "master";
inline juce::String trackGain(int t) { return "gain" + juce::String(t); }
} // namespace ids

// Names map row -> synthesised voice (see DrumSynth + prepareToPlay).
inline const std::array<const char*, 8> kTrackNames {
    "Kick", "Snare", "CH", "OH", "Clap", "Tom", "Rim", "Cowb"
};

// A structural edit travelling UI thread -> audio thread through the lock-free
// queue. Toggling a step is not a continuous parameter, so APVTS isn't the right
// home for it; a command queue is.
struct EditCommand
{
    enum class Type { toggleCell, clearAll };
    Type  type   = Type::toggleCell;
    int   track  = 0;
    int   step   = 0;
    float velocity = 0.8f;
};

class GridSeqAudioProcessor : public juce::AudioProcessor
{
public:
    GridSeqAudioProcessor();
    ~GridSeqAudioProcessor() override = default;

    // -- AudioProcessor real-time surface -------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // -- Editor ---------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // -- Identity / boilerplate -----------------------------------------------
    const juce::String getName() const override { return "GridSeq"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "GridSeq"; }
    void changeProgramName(int, const juce::String&) override {}

    // -- State persistence ----------------------------------------------------
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // -- Bridges for the editor -----------------------------------------------
    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }

    // Editor -> audio thread (producer side of the SPSC queue). Never blocks; a
    // full queue simply drops the edit (the UI will resend on the next click).
    void postEdit(const EditCommand& cmd) noexcept { edits_.push(cmd); }

    // UI reads (display only): the playhead column, and the pattern for drawing.
    int currentStep() const noexcept { return playhead_.load(std::memory_order_relaxed); }
    const gridseq::Pattern& pattern() const noexcept { return engine_.pattern(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void drainEdits() noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Cached atomic pointers to the raw parameter values. Reading these in the
    // audio callback is the standard JUCE lock-free path for continuous params.
    std::atomic<float>* runParam_   = nullptr;
    std::atomic<float>* tempoParam_ = nullptr;
    std::atomic<float>* swingParam_ = nullptr;
    std::atomic<float>* masterParam_ = nullptr;
    std::array<std::atomic<float>*, 8> gainParams_ {};

    gridseq::SequencerEngine engine_;
    gridseq::LockFreeFifo<EditCommand, 1024> edits_;
    std::atomic<int> playhead_ { 0 };

    // Pre-allocated stereo scratch so the engine always renders into two distinct
    // buffers regardless of the host's channel count - no allocation in the
    // callback.
    juce::AudioBuffer<float> scratch_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridSeqAudioProcessor)
};
