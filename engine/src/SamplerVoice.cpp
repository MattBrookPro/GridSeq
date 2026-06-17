#include "gridseq/SamplerVoice.h"

namespace gridseq {

void SamplerVoice::trigger(int offsetInBlock, float gain) noexcept
{
    // Restart from the top - drum-pad behaviour. We only stamp the start offset
    // and reset state; no allocation, so this is safe on the audio thread.
    playPos_     = 0;
    startOffset_ = offsetInBlock < 0 ? 0 : offsetInBlock;
    gain_        = gain;
    active_      = true;
}

void SamplerVoice::render(float* outL, float* outR, int numSamples) noexcept
{
    if (! active_ || sample_ == nullptr || sample_->empty())
        return;

    const std::size_t len = sample_->size();

    // Begin at the trigger offset on the block in which we were triggered; every
    // subsequent block starts at 0 (startOffset_ is cleared at the end).
    for (int i = startOffset_; i < numSamples; ++i)
    {
        const float s = (*sample_)[playPos_] * gain_;
        outL[i] += s; // additive: the engine sums all voices into one buffer
        outR[i] += s; // mono source mirrored to both channels

        if (++playPos_ >= len)
        {
            active_ = false; // one-shot finished
            break;
        }
    }

    startOffset_ = 0;
}

} // namespace gridseq
