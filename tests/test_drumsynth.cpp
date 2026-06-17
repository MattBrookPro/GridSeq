#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "gridseq/DrumSynth.h"

using namespace gridseq;

// Characterisation tests for the procedural kit. We can't assert "this sounds
// like a kick", but we CAN guard the properties that matter: every voice has
// sensible length, actually makes sound, and never clips the [-1, 1] range
// (so summing a few of them stays sane). These also pin the kit as a stable,
// reproducible fixture for the offline render tool.

namespace
{
struct NamedVoice { const char* name; std::vector<float> buf; };

std::vector<NamedVoice> makeKit(double sr)
{
    return {
        { "kick",      drums::kick(sr) },
        { "snare",     drums::snare(sr) },
        { "closedHat", drums::closedHat(sr) },
        { "openHat",   drums::openHat(sr) },
        { "clap",      drums::clap(sr) },
        { "tom",       drums::tom(sr) },
        { "rim",       drums::rim(sr) },
        { "cowbell",   drums::cowbell(sr) },
    };
}

float peak(const std::vector<float>& b)
{
    float p = 0.0f;
    for (float s : b) p = std::max(p, std::fabs(s));
    return p;
}
} // namespace

TEST_CASE("every kit voice has sensible length, makes sound, and never clips", "[drums]")
{
    const double sr = 48000.0;
    for (const auto& v : makeKit(sr))
    {
        INFO("voice: " << v.name);
        CHECK(v.buf.size() > 100);                  // not empty / not a click of nothing
        CHECK(v.buf.size() < static_cast<std::size_t>(sr)); // under a second
        CHECK(peak(v.buf) > 0.05f);                 // audible
        CHECK(peak(v.buf) <= 1.0f);                 // headroom preserved, no clipping
    }
}

TEST_CASE("synthesis is deterministic (stable regression fixture)", "[drums]")
{
    // Bit-identical on repeat - required for the offline-render regression check.
    CHECK(drums::snare(48000.0) == drums::snare(48000.0));
    CHECK(drums::closedHat(44100.0) == drums::closedHat(44100.0));
}

TEST_CASE("length scales with sample rate", "[drums]")
{
    CHECK(drums::kick(96000.0).size() > drums::kick(48000.0).size());
}
