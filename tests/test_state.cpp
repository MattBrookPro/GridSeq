#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

#include "gridseq/SequencerEngine.h"
#include "gridseq/StateCodec.h"

using namespace gridseq;
using Catch::Matchers::WithinAbs;

// Saving and recalling a kit/pattern is table stakes for an instrument, and it
// is pure data logic, so it is a perfect unit-test target. The contract: after
// deserialize(serialize(e)) a fresh engine must be indistinguishable from the
// original - tempo, swing, geometry, per-track gain and every cell.

namespace
{
void buildKit(SequencerEngine& e)
{
    e.prepare(44100.0);
    e.setTempo(140.5);
    e.setSwing(0.33);
    e.pattern().setNumTracks(6);
    e.pattern().setNumSteps(12);
    e.setTrackGain(0, 0.75f);
    e.setTrackGain(3, 0.20f);
    e.pattern().setCell(0, 0, 1.0f);
    e.pattern().setCell(0, 4, 0.6f);
    e.pattern().setCell(2, 7, 0.9f);
    e.pattern().setCell(5, 11, 0.5f);
}
} // namespace

TEST_CASE("state survives a serialize/deserialize round-trip", "[state]")
{
    SequencerEngine original;
    buildKit(original);

    const std::string blob = serialize(original);
    REQUIRE_FALSE(blob.empty());

    SequencerEngine restored;
    REQUIRE(deserialize(restored, blob));

    // Spot-check the scalar fields through the public API...
    CHECK_THAT(restored.getTempo(), WithinAbs(140.5, 1e-6));
    CHECK_THAT(restored.getSwing(), WithinAbs(0.33, 1e-6));
    CHECK(restored.pattern().numTracks() == 6);
    CHECK(restored.pattern().numSteps() == 12);
    CHECK_THAT(restored.pattern().cell(0, 4), WithinAbs(0.6f, 1e-6));
    CHECK_THAT(restored.pattern().cell(2, 7), WithinAbs(0.9f, 1e-6));
    CHECK_FALSE(restored.pattern().isActive(1, 1));

    // ...and prove the WHOLE state matches by re-serialising: identical bytes
    // means gains, cells and geometry all round-tripped exactly.
    CHECK(serialize(restored) == blob);
}

TEST_CASE("a default engine round-trips", "[state]")
{
    SequencerEngine a;
    a.prepare(44100.0);
    SequencerEngine b;
    REQUIRE(deserialize(b, serialize(a)));
    CHECK(serialize(b) == serialize(a));
}

TEST_CASE("deserialize rejects malformed or foreign data", "[state]")
{
    SequencerEngine e;
    e.prepare(44100.0);

    CHECK_FALSE(deserialize(e, ""));
    CHECK_FALSE(deserialize(e, "not a gridseq blob"));
    CHECK_FALSE(deserialize(e, "GRIDSEQ 999\nbpm 120\n")); // unsupported version
}
