#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "gridseq/Pattern.h"
#include "gridseq/SamplerVoice.h"

namespace gridseq {

// -- SequencerEngine -----------------------------------------------------------
//
// The musical core: a step sequencer driving one sampler voice per track. It is
// deliberately free of any JUCE / GUI / audio-device dependency. That absence is
// the architectural keystone of the whole project:
//
//   * It can be unit-tested in isolation - no audio hardware, no message loop.
//     A test prepares the engine, renders a block, and asserts which steps fired
//     at which sample offsets.
//   * The UI becomes swappable - JUCE today, anything tomorrow.
//
// TIMING MODEL (the subtle part):
//   We keep a monotonically increasing absolute step index. The "straight" grid
//   time of a step is  index * samplesPerStep.  Swing is applied as an *offset*
//   added to odd steps, computed from that grid each time rather than
//   accumulated, so it can never drift. Each processBlock walks every step whose
//   fire-time falls inside the block and triggers it at the exact sample offset.
class SequencerEngine
{
public:
    // -- Lifecycle ------------------------------------------------------------
    void prepare(double sampleRate) noexcept;
    void reset() noexcept; // rewind transport to step 0

    // -- Transport ------------------------------------------------------------
    void setPlaying(bool shouldPlay) noexcept;
    bool isPlaying() const noexcept { return playing_; }

    // -- Musical parameters ---------------------------------------------------
    // These are plain values. Delivering them to the audio thread without locks
    // is the *plugin layer's* job (atomics for these scalars, an SPSC queue for
    // structural pattern edits). Keeping the engine free of that machinery is
    // what keeps it pure and testable.
    void   setTempo(double bpm) noexcept;
    double getTempo() const noexcept { return bpm_; }

    // Swing in [0, kMaxSwing]: fraction of a step that odd (off-beat) 16ths are
    // pushed later. 0 = straight, ~0.66 = heavy shuffle.
    static constexpr double kMaxSwing = 0.66;
    void   setSwing(double amount) noexcept;
    double getSwing() const noexcept { return swing_; }

    Pattern&       pattern() noexcept       { return pattern_; }
    const Pattern& pattern() const noexcept { return pattern_; }

    // Per-track sample data (mono). The engine keeps its own copy so the voice's
    // non-owning pointer stays valid. Call off the audio thread (it allocates).
    void setSample(int track, std::vector<float> monoSample);
    void setTrackGain(int track, float gain) noexcept;

    // -- Real-time render ------------------------------------------------------
    // Renders `numSamples` of stereo audio. MUST be allocation-free and lock-free.
    void processBlock(float* outL, float* outR, int numSamples) noexcept;

    // For the UI playhead. Written on the audio thread, read on the UI thread -
    // a single relaxed atomic is the standard lock-free way to publish one scalar.
    int currentStep() const noexcept { return displayStep_.load(std::memory_order_relaxed); }

    // -- State (de)serialisation lives in StateCodec, declared as friends so the
    // codec can read private fields without widening the public surface.
    friend std::string serialize(const SequencerEngine&);
    friend bool        deserialize(SequencerEngine&, const std::string&);

private:
    // Fire-time of an absolute step index, in samples on the transport timeline.
    double fireTimeSamples(std::int64_t absStepIndex) const noexcept;
    void   recomputeStepLength() noexcept;
    void   triggerStep(int patternStep, int offsetInBlock) noexcept;
    void   renderVoices(float* outL, float* outR, int numSamples) noexcept;

    double sampleRate_   = 44100.0;
    double bpm_          = 120.0;
    double swing_        = 0.0;
    int    stepsPerBeat_ = 4;       // 16th-note resolution
    bool   playing_      = false;

    double       samplesPerStep_  = 0.0;
    std::int64_t absStepIndex_    = 0;  // monotonic; %numSteps gives the pattern column
    std::int64_t transportSamples_ = 0; // samples elapsed since play started

    Pattern                                  pattern_;
    std::array<SamplerVoice, Pattern::kMaxTracks> voices_{};
    std::array<std::vector<float>, Pattern::kMaxTracks> samples_{};
    std::array<float, Pattern::kMaxTracks>   trackGain_{};

    std::atomic<int> displayStep_{0};
};

} // namespace gridseq
