#pragma once

#include <cstddef>
#include <vector>

namespace gridseq {

// One monophonic sampler voice - the "MPC pad" of a single track. It plays a
// mono sample buffer from the start each time it is triggered (one-shot, retrig
// on re-trigger), which is exactly how a drum pad behaves.
//
// REAL-TIME CONTRACT:
//  - The voice never owns or allocates its sample data. It holds a non-owning
//    pointer to a buffer owned by the engine. Swapping that buffer is a setup
//    operation done off the audio thread; render()/trigger() only read it.
//  - trigger() and render() are noexcept and allocation-free, so they are safe
//    to call from the audio callback.
class SamplerVoice
{
public:
    // Point the voice at a sample buffer (owned elsewhere, must outlive the voice).
    // Passing nullptr makes the voice silent. Call off the audio thread.
    void setSample(const std::vector<float>* monoSample) noexcept { sample_ = monoSample; }

    // Start playback. `offsetInBlock` is the sample position *within the current
    // render block* at which the hit should sound - this is what makes triggers
    // land sample-accurately rather than being quantised to block boundaries.
    void trigger(int offsetInBlock, float gain) noexcept;

    // Mix this voice's output into the stereo block (additive). Honours the
    // pending start offset on the block in which it was triggered.
    void render(float* outL, float* outR, int numSamples) noexcept;

    bool isActive() const noexcept { return active_; }
    void stop() noexcept { active_ = false; }

private:
    const std::vector<float>* sample_ = nullptr;
    std::size_t playPos_   = 0;     // next sample index to read
    int         startOffset_ = 0;   // where in *this* block to begin (then 0)
    float       gain_      = 1.0f;
    bool        active_    = false;
};

} // namespace gridseq
