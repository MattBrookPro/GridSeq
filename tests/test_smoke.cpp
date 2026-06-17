#include <catch2/catch_test_macros.hpp>

#include "gridseq/Pattern.h"

using namespace gridseq;

// A minimal sanity check that proves the test harness, the Catch2 fetch, and the
// link against the engine library all work end-to-end before any real behaviour
// tests are written. The behaviour specs arrive in the following commits.
TEST_CASE("Pattern toggles a cell on and off", "[pattern][smoke]")
{
    Pattern p;
    REQUIRE_FALSE(p.isActive(0, 0));

    p.toggle(0, 0);
    REQUIRE(p.isActive(0, 0));

    p.toggle(0, 0);
    REQUIRE_FALSE(p.isActive(0, 0));
}
