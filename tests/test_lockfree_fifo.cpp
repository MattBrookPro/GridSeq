#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "gridseq/LockFreeFifo.h"

using namespace gridseq;

// The single-threaded semantics we CAN pin down deterministically: ordering,
// the full/empty boundary, and wrap-around. (Cross-thread correctness is a
// property of the memory ordering, argued in the header, not unit-tested here.)

TEST_CASE("fifo preserves push order on pop", "[fifo]")
{
    LockFreeFifo<int, 8> q;
    for (int i = 0; i < 6; ++i)
        REQUIRE(q.push(i));

    std::vector<int> got;
    int v;
    while (q.pop(v))
        got.push_back(v);

    CHECK(got == std::vector<int>{ 0, 1, 2, 3, 4, 5 });
}

TEST_CASE("fifo reports full at usable capacity (N-1)", "[fifo]")
{
    LockFreeFifo<int, 4> q; // usable capacity == 3
    CHECK(q.capacity() == 3);

    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));
    CHECK_FALSE(q.push(4)); // full: never overwrites, never blocks
}

TEST_CASE("popping an empty fifo returns false and leaves the target alone", "[fifo]")
{
    LockFreeFifo<int, 4> q;
    CHECK(q.empty());

    int sentinel = 42;
    CHECK_FALSE(q.pop(sentinel));
    CHECK(sentinel == 42);
}

TEST_CASE("empty() tracks the queue state", "[fifo]")
{
    LockFreeFifo<int, 4> q;
    CHECK(q.empty());
    q.push(7);
    CHECK_FALSE(q.empty());
    int v;
    q.pop(v);
    CHECK(q.empty());
}

TEST_CASE("fifo wraps around correctly over many cycles", "[fifo]")
{
    LockFreeFifo<int, 4> q; // small, so the indices wrap repeatedly
    for (int i = 0; i < 1000; ++i)
    {
        REQUIRE(q.push(i));
        int v = -1;
        REQUIRE(q.pop(v));
        REQUIRE(v == i); // value out matches value in, cycle after cycle
    }
    CHECK(q.empty());
}
