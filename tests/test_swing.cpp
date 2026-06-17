#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "gridseq/SequencerEngine.h"

using namespace gridseq;
using Catch::Matchers::WithinAbs;

// Swing/shuffle is the instrument's "feel" control and its one bit of bespoke
// musical maths, so it gets its own spec. The groove rule: off-beat (odd) 16th
// notes are pushed LATER by `swing` fractions of a step; on-beat (even) steps
// never move. At 48k/120bpm a step is 6000 samples, so swing 0.5 pushes odd
// steps by exactly 3000 samples.
namespace
{
constexpr double kSR  = 48000.0;
constexpr int    kSPS = 6000;

std::vector<int> hitOffsets(const std::vector<float>& buf)
{
    std::vector<int> hits;
    for (int i = 0; i < static_cast<int>(buf.size()); ++i)
        if (buf[static_cast<std::size_t>(i)] != 0.0f)
            hits.push_back(i);
    return hits;
}

void configure(SequencerEngine& e)
{
    e.prepare(kSR);
    e.setTempo(120.0);
    for (int t = 0; t < Pattern::kMaxTracks; ++t)
        e.setSample(t, { 1.0f });
}
} // namespace

TEST_CASE("swing pushes odd steps later, leaves even steps put", "[swing]")
{
    SequencerEngine e;
    configure(e);
    e.setSwing(0.5); // half-step shuffle
    for (int s = 0; s < 4; ++s)
        e.pattern().setCell(0, s, 1.0f);
    e.setPlaying(true);

    std::vector<float> l(4 * kSPS, 0.0f), r(4 * kSPS, 0.0f);
    e.processBlock(l.data(), r.data(), static_cast<int>(l.size()));

    // step0=0, step1=6000+3000, step2=12000, step3=18000+3000
    CHECK(hitOffsets(l)
          == std::vector<int>{ 0, kSPS + 3000, 2 * kSPS, 3 * kSPS + 3000 });
}

TEST_CASE("swing of zero is a perfectly straight grid", "[swing]")
{
    SequencerEngine e;
    configure(e);
    e.setSwing(0.0);
    for (int s = 0; s < 4; ++s)
        e.pattern().setCell(0, s, 1.0f);
    e.setPlaying(true);

    std::vector<float> l(4 * kSPS, 0.0f), r(4 * kSPS, 0.0f);
    e.processBlock(l.data(), r.data(), static_cast<int>(l.size()));

    CHECK(hitOffsets(l) == std::vector<int>{ 0, kSPS, 2 * kSPS, 3 * kSPS });
}

TEST_CASE("swing amount is clamped to the musical maximum", "[swing]")
{
    SequencerEngine e;
    configure(e);
    e.setSwing(10.0); // absurd request
    CHECK_THAT(e.getSwing(), WithinAbs(SequencerEngine::kMaxSwing, 1e-9));

    e.setSwing(-1.0);
    CHECK_THAT(e.getSwing(), WithinAbs(0.0, 1e-9));
}

TEST_CASE("swing does not accumulate drift over many bars", "[swing]")
{
    SequencerEngine e;
    configure(e);
    e.setSwing(0.33);
    e.pattern().setNumSteps(2);
    e.pattern().setCell(0, 0, 1.0f); // only the on-beat fires
    e.setPlaying(true);

    // Render 100 bars' worth in one block. The even on-beats must remain on the
    // exact straight grid (multiples of 2*kSPS), proving swing is an offset, not
    // an accumulator.
    const int bars = 100;
    std::vector<float> l(static_cast<std::size_t>(bars * 2 * kSPS), 0.0f);
    std::vector<float> r(l.size(), 0.0f);
    e.processBlock(l.data(), r.data(), static_cast<int>(l.size()));

    const auto hits = hitOffsets(l);
    REQUIRE(hits.size() == static_cast<std::size_t>(bars));
    for (int b = 0; b < bars; ++b)
        CHECK(hits[static_cast<std::size_t>(b)] == b * 2 * kSPS);
}
