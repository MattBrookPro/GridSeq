#include "gridseq/SequencerEngine.h"

#include <algorithm>

namespace gridseq {

void SequencerEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (int t = 0; t < Pattern::kMaxTracks; ++t)
    {
        voices_[static_cast<std::size_t>(t)].setSample(&samples_[static_cast<std::size_t>(t)]);
        if (trackGain_[static_cast<std::size_t>(t)] == 0.0f)
            trackGain_[static_cast<std::size_t>(t)] = 1.0f;
    }
    recomputeStepLength();
    reset();
}

void SequencerEngine::reset() noexcept
{
    absStepIndex_     = 0;
    transportSamples_ = 0;
    displayStep_.store(0, std::memory_order_relaxed);
    for (auto& v : voices_)
        v.stop();
}

void SequencerEngine::setPlaying(bool shouldPlay) noexcept
{
    if (shouldPlay && ! playing_)
        reset();
    playing_ = shouldPlay;
}

void SequencerEngine::setTempo(double bpm) noexcept
{
    bpm_ = std::clamp(bpm, 20.0, 999.0);
    recomputeStepLength();
}

void SequencerEngine::setSwing(double amount) noexcept
{
    swing_ = std::clamp(amount, 0.0, kMaxSwing);
}

void SequencerEngine::setSample(int track, std::vector<float> monoSample)
{
    if (track < 0 || track >= Pattern::kMaxTracks)
        return;
    samples_[static_cast<std::size_t>(track)] = std::move(monoSample);
    voices_[static_cast<std::size_t>(track)].setSample(&samples_[static_cast<std::size_t>(track)]);
}

void SequencerEngine::setTrackGain(int track, float gain) noexcept
{
    if (track < 0 || track >= Pattern::kMaxTracks)
        return;
    trackGain_[static_cast<std::size_t>(track)] = std::max(0.0f, gain);
}

void SequencerEngine::recomputeStepLength() noexcept
{
    const double secondsPerBeat = 60.0 / bpm_;
    samplesPerStep_ = secondsPerBeat / stepsPerBeat_ * sampleRate_;
}

double SequencerEngine::fireTimeSamples(std::int64_t absStepIndex) const noexcept
{
    // Straight-grid time of this step on the transport timeline...
    const double straight = static_cast<double>(absStepIndex) * samplesPerStep_;

    // ...plus swing. Swing pushes the odd (off-beat) 16ths later by a fraction
    // of a step. Computing it as an offset off the straight grid EACH TIME -
    // rather than accumulating it - means rounding can never compound, so the
    // groove stays locked over arbitrarily long playback. (numSteps is even, so
    // odd absolute index == odd column, keeping the shuffle consistent on wrap.)
    const bool isOffBeat = (absStepIndex % 2) != 0;
    const double swingOffset = isOffBeat ? swing_ * samplesPerStep_ : 0.0;
    return straight + swingOffset;
}

void SequencerEngine::triggerStep(int patternStep, int offsetInBlock) noexcept
{
    for (int t = 0; t < pattern_.numTracks(); ++t)
    {
        const float velocity = pattern_.cell(t, patternStep);
        if (velocity <= 0.0f)
            continue; // step is off on this track

        const auto ut = static_cast<std::size_t>(t);
        voices_[ut].trigger(offsetInBlock, trackGain_[ut] * velocity);
    }
    // Publish the playhead for the UI (audio thread writes, UI reads).
    displayStep_.store(patternStep, std::memory_order_relaxed);
}

void SequencerEngine::renderVoices(float* outL, float* outR, int numSamples) noexcept
{
    for (auto& v : voices_)
        v.render(outL, outR, numSamples);
}

void SequencerEngine::processBlock(float* outL, float* outR, int numSamples) noexcept
{
    // Always start from a clean buffer; voices mix in additively.
    for (int i = 0; i < numSamples; ++i)
    {
        outL[i] = 0.0f;
        outR[i] = 0.0f;
    }

    if (! playing_ || samplesPerStep_ <= 0.0)
    {
        // Still render so any note tails from before the stop ring out.
        renderVoices(outL, outR, numSamples);
        return;
    }

    const double blockStart = static_cast<double>(transportSamples_);
    const double blockEnd    = blockStart + numSamples;

    // Sample-accurate event handling: render the audio in segments split at each
    // step boundary. WHY not "trigger everything, then render once": a track's
    // voice is monophonic, so two hits of the same track inside one block would
    // collapse onto the last trigger. Rendering [cursor, offset) before applying
    // each trigger places every hit at its true sample position. A re-trigger
    // therefore chokes the previous one-shot (MPC-style mono pad) - a deliberate
    // choice; per-track polyphony would be the extension if overlap were wanted.
    int cursor = 0; // samples already rendered within this block

    while (fireTimeSamples(absStepIndex_) < blockEnd)
    {
        const double fire = fireTimeSamples(absStepIndex_);
        int offset = static_cast<int>(fire - blockStart + 0.5); // round to sample
        if (offset < cursor)        offset = cursor;
        if (offset > numSamples)    offset = numSamples;

        // Render up to the trigger point, then fire at the segment's start.
        renderVoices(outL + cursor, outR + cursor, offset - cursor);

        const int patternStep = static_cast<int>(absStepIndex_ % pattern_.numSteps());
        triggerStep(patternStep, /*offsetInBlock within next segment*/ 0);

        cursor = offset;
        ++absStepIndex_;
    }

    // Render the remaining tail of the block after the last trigger.
    renderVoices(outL + cursor, outR + cursor, numSamples - cursor);

    transportSamples_ += numSamples;
}

} // namespace gridseq
