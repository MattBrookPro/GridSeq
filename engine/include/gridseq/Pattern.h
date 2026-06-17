#pragma once

#include <array>
#include <cstdint>

namespace gridseq {

// A Pattern is the grid a beat-maker actually edits: `numTracks` rows (one per
// sampler voice) by `numSteps` columns. Each cell holds a *velocity* in [0, 1];
// a velocity of 0 means the step is off. Storing velocity rather than a plain
// on/off bool costs nothing here and buys musical expression (accents, ghost
// notes) plus something meaningful for the humanise/swing feature to act on.
//
// WHY fixed-size std::array rather than std::vector: the Pattern is read on the
// real-time audio thread. Fixed storage means no heap allocation ever happens
// while editing or reading cells, so touching the grid can never cause an
// audio-thread allocation. The cost is a hard cap (kMaxTracks x kMaxSteps),
// which is fine for an instrument of this size.
class Pattern
{
public:
    static constexpr int kMaxTracks = 8;
    static constexpr int kMaxSteps  = 32;

    Pattern() { clear(); }

    void clear() noexcept;

    // Geometry. Values are clamped into the valid range; out-of-range requests
    // are ignored rather than throwing, because callers include the audio thread.
    void setNumTracks(int tracks) noexcept;
    void setNumSteps(int steps) noexcept;
    int  numTracks() const noexcept { return numTracks_; }
    int  numSteps()  const noexcept { return numSteps_; }

    // Cell access. setCell with velocity == 0 turns the step off.
    void  setCell(int track, int step, float velocity) noexcept;
    float cell(int track, int step) const noexcept;
    bool  isActive(int track, int step) const noexcept { return cell(track, step) > 0.0f; }

    // Convenience used by the UI: flip a step between off and a default accent.
    void toggle(int track, int step, float onVelocity = 0.8f) noexcept;

private:
    static bool inRange(int track, int step) noexcept;

    int numTracks_ = kMaxTracks;
    int numSteps_  = 16; // one bar of 16th notes by default - classic MPC grid
    std::array<std::array<float, kMaxSteps>, kMaxTracks> cells_{};
};

} // namespace gridseq
