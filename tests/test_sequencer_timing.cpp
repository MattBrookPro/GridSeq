#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <vector>

#include "gridseq/SequencerEngine.h"

using namespace gridseq;
using Catch::Matchers::WithinAbs;

// Specification for the heart of the instrument: WHICH step fires, and at WHICH
// sample offset, when the engine renders a block. We observe triggers through
// the rendered audio - a single-sample impulse on a track means "wherever the
// output is non-zero, that track was triggered on that exact sample".
//
// Sample-rate/tempo are chosen so the maths is exact and human-checkable:
//   48000 Hz, 120 BPM, 16th-note grid (4 steps/beat)
//   => seconds-per-step = (60/120)/4 = 0.125 s => 6000 samples per step.
namespace
{
constexpr double kSR  = 48000.0;
constexpr int    kSPS = 6000; // samples per 16th-note step at 48k/120bpm

// Indices in `buf` that are non-zero (i.e. where a hit landed).
std::vector<int> hitOffsets(const std::vector<float>& buf)
{
    std::vector<int> hits;
    for (int i = 0; i < static_cast<int>(buf.size()); ++i)
        if (buf[static_cast<std::size_t>(i)] != 0.0f)
            hits.push_back(i);
    return hits;
}

// Configure by reference: SequencerEngine holds a std::atomic (for the UI
// playhead) so it is intentionally non-movable - exactly how it lives as a
// plugin member. So we set one up in place rather than returning by value.
void configure(SequencerEngine& e)
{
    e.prepare(kSR);
    e.setTempo(120.0);
    e.setSwing(0.0);
    // A one-sample impulse on each track lets us read trigger positions directly.
    for (int t = 0; t < Pattern::kMaxTracks; ++t)
        e.setSample(t, { 1.0f });
}
} // namespace

TEST_CASE("step 0 fires on the first sample when playback starts", "[timing]")
{
    SequencerEngine e;
    configure(e);
    e.pattern().setCell(0, 0, 1.0f);
    e.setPlaying(true);

    std::vector<float> l(16, 0.0f), r(16, 0.0f);
    e.processBlock(l.data(), r.data(), 16);

    CHECK_THAT(l[0], WithinAbs(1.0f, 1e-6));
    CHECK(hitOffsets(l) == std::vector<int>{ 0 });
}

TEST_CASE("consecutive steps are spaced one step-length apart", "[timing]")
{
    SequencerEngine e;
    configure(e);
    e.pattern().setCell(0, 0, 1.0f);
    e.pattern().setCell(0, 1, 1.0f);
    e.pattern().setCell(0, 2, 1.0f);
    e.setPlaying(true);

    std::vector<float> l(2 * kSPS + 1, 0.0f), r(2 * kSPS + 1, 0.0f);
    e.processBlock(l.data(), r.data(), static_cast<int>(l.size()));

    CHECK(hitOffsets(l) == std::vector<int>{ 0, kSPS, 2 * kSPS });
}

TEST_CASE("a step beyond the block boundary fires in the next block", "[timing]")
{
    SequencerEngine e;
    configure(e);
    e.pattern().setCell(0, 0, 1.0f);
    e.pattern().setCell(0, 1, 1.0f);
    e.setPlaying(true);

    // Block 1 spans [0, 6000): step 0 fires at 0, step 1 at 6000 is NOT < 6000.
    std::vector<float> l(kSPS, 0.0f), r(kSPS, 0.0f);
    e.processBlock(l.data(), r.data(), kSPS);
    CHECK(hitOffsets(l) == std::vector<int>{ 0 });

    // Block 2 spans [6000, 12000): step 1 now fires at local offset 0.
    std::fill(l.begin(), l.end(), 0.0f);
    e.processBlock(l.data(), r.data(), kSPS);
    CHECK(hitOffsets(l) == std::vector<int>{ 0 });
}

TEST_CASE("pattern wraps around after numSteps", "[timing]")
{
    SequencerEngine e;
    configure(e);
    e.pattern().setNumSteps(4);
    for (int s = 0; s < 4; ++s)
        e.pattern().setCell(0, s, 1.0f);
    e.setPlaying(true);

    // 4 steps then wrap: expect hits at 0,1,2,3 * SPS and step-0-again at 4*SPS.
    std::vector<float> l(4 * kSPS + 1, 0.0f), r(4 * kSPS + 1, 0.0f);
    e.processBlock(l.data(), r.data(), static_cast<int>(l.size()));

    CHECK(hitOffsets(l)
          == std::vector<int>{ 0, kSPS, 2 * kSPS, 3 * kSPS, 4 * kSPS });
}

TEST_CASE("a stopped engine produces silence", "[timing]")
{
    SequencerEngine e;
    configure(e);
    e.pattern().setCell(0, 0, 1.0f);
    e.setPlaying(false);

    std::vector<float> l(64, 0.0f), r(64, 0.0f);
    e.processBlock(l.data(), r.data(), 64);

    CHECK(hitOffsets(l).empty());
}

TEST_CASE("velocity and track gain scale the triggered hit", "[timing]")
{
    SequencerEngine e;
    configure(e);
    e.pattern().setCell(0, 0, 0.5f);  // half-velocity accent
    e.setTrackGain(0, 0.5f);          // and half track gain
    e.setPlaying(true);

    std::vector<float> l(16, 0.0f), r(16, 0.0f);
    e.processBlock(l.data(), r.data(), 16);

    CHECK_THAT(l[0], WithinAbs(0.25f, 1e-6)); // 0.5 velocity * 0.5 gain
}

TEST_CASE("currentStep reports the most recent step for the UI playhead", "[timing]")
{
    SequencerEngine e;
    configure(e);
    for (int s = 0; s < 16; ++s)
        e.pattern().setCell(0, s, 1.0f);
    e.setPlaying(true);

    std::vector<float> l(2 * kSPS + 1, 0.0f), r(2 * kSPS + 1, 0.0f);
    e.processBlock(l.data(), r.data(), static_cast<int>(l.size()));

    CHECK(e.currentStep() == 2); // steps 0,1,2 fired in this block
}
