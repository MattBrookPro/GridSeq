#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <vector>

#include "gridseq/SamplerVoice.h"

using namespace gridseq;
using Catch::Matchers::WithinAbs;

// These tests are written BEFORE SamplerVoice is implemented. They are the
// specification: a sampler voice must place its sample into the output buffer
// sample-accurately (at the requested offset), play one-shot to completion,
// scale by gain, sum additively into stereo, and restart on re-trigger.

namespace
{
// A recognisable impulse so we can read back exactly where it landed.
const std::vector<float> kImpulse = { 1.0f, 0.5f, 0.25f };
} // namespace

TEST_CASE("voice plays the sample from the start of the block at offset 0", "[voice]")
{
    SamplerVoice v;
    v.setSample(&kImpulse);
    v.trigger(/*offsetInBlock*/ 0, /*gain*/ 1.0f);

    std::vector<float> l(8, 0.0f), r(8, 0.0f);
    v.render(l.data(), r.data(), 8);

    CHECK_THAT(l[0], WithinAbs(1.0f, 1e-6));
    CHECK_THAT(l[1], WithinAbs(0.5f, 1e-6));
    CHECK_THAT(l[2], WithinAbs(0.25f, 1e-6));
    CHECK_THAT(l[3], WithinAbs(0.0f, 1e-6)); // sample exhausted
    // Mono sample is mirrored to both channels.
    CHECK_THAT(r[0], WithinAbs(1.0f, 1e-6));
}

TEST_CASE("trigger offset is sample-accurate within the block", "[voice]")
{
    SamplerVoice v;
    v.setSample(&kImpulse);
    v.trigger(/*offsetInBlock*/ 3, 1.0f);

    std::vector<float> l(8, 0.0f), r(8, 0.0f);
    v.render(l.data(), r.data(), 8);

    CHECK_THAT(l[0], WithinAbs(0.0f, 1e-6));
    CHECK_THAT(l[2], WithinAbs(0.0f, 1e-6));
    CHECK_THAT(l[3], WithinAbs(1.0f, 1e-6)); // hit lands exactly on sample 3
    CHECK_THAT(l[4], WithinAbs(0.5f, 1e-6));
}

TEST_CASE("gain scales the sample", "[voice]")
{
    SamplerVoice v;
    v.setSample(&kImpulse);
    v.trigger(0, 0.5f);

    std::vector<float> l(4, 0.0f), r(4, 0.0f);
    v.render(l.data(), r.data(), 4);

    CHECK_THAT(l[0], WithinAbs(0.5f, 1e-6));
    CHECK_THAT(l[1], WithinAbs(0.25f, 1e-6));
}

TEST_CASE("voice mixes additively and goes inactive after one shot", "[voice]")
{
    SamplerVoice v;
    v.setSample(&kImpulse);

    std::vector<float> l(4, 10.0f), r(4, 10.0f); // pre-loaded to prove additive mix
    v.trigger(0, 1.0f);
    v.render(l.data(), r.data(), 4);

    CHECK_THAT(l[0], WithinAbs(11.0f, 1e-6)); // 10 + 1
    CHECK_FALSE(v.isActive());                // 3-sample one-shot finished in a 4-sample block
}

TEST_CASE("playback spans across multiple blocks", "[voice]")
{
    const std::vector<float> longSample(10, 1.0f);
    SamplerVoice v;
    v.setSample(&longSample);
    v.trigger(0, 1.0f);

    std::vector<float> l(4, 0.0f), r(4, 0.0f);
    v.render(l.data(), r.data(), 4);
    CHECK(v.isActive()); // 10-sample voice cannot finish in 4 samples

    // Second block continues from where the first left off (no offset re-apply).
    std::fill(l.begin(), l.end(), 0.0f);
    v.render(l.data(), r.data(), 4);
    CHECK_THAT(l[0], WithinAbs(1.0f, 1e-6));
    CHECK(v.isActive()); // 8 played, 2 remain
}

TEST_CASE("re-trigger restarts playback from the top", "[voice]")
{
    SamplerVoice v;
    v.setSample(&kImpulse);

    std::vector<float> l(4, 0.0f), r(4, 0.0f);
    v.trigger(0, 1.0f);
    v.render(l.data(), r.data(), 2); // play first two samples only
    v.trigger(0, 1.0f);              // restart
    std::fill(l.begin(), l.end(), 0.0f);
    v.render(l.data(), r.data(), 4);
    CHECK_THAT(l[0], WithinAbs(1.0f, 1e-6)); // back at sample 0
}
