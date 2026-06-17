#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace gridseq {

// -- LockFreeFifo --------------------------------------------------------------
//
// A bounded single-producer / single-consumer queue. This is how structural
// edits (e.g. "toggle step 5 on track 2") cross from the UI thread to the audio
// thread WITHOUT a mutex.
//
// WHY this matters: if the audio callback ever
// took a lock that the UI thread held, it could block - and a blocked audio
// thread is an instant dropout/glitch. A lock-free ring lets the producer publish
// and the consumer drain with only atomic loads/stores, so the audio thread never
// waits on anyone.
//
// CONTRACT: exactly one thread calls push(), exactly one calls pop() (SPSC).
//   - Capacity must be a power of two so index wrap is a cheap bit-mask.
//   - One slot is kept empty to tell "full" from "empty", so usable size is N-1.
//
// NOTE FOR THE READER: a unit test can only prove the single-threaded semantics
// (order, full/empty). True race-freedom rests on the acquire/release memory
// ordering below, which is reasoned about, not deterministically unit-tested.
template <typename T, std::size_t Capacity>
class LockFreeFifo
{
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two and >= 2");

public:
    // Producer side. Returns false if the queue is full (caller decides whether
    // to drop or retry - we never block).
    bool push(const T& item) noexcept
    {
        // The producer owns writeIndex_, so a relaxed load of its own index is
        // fine. We must see the consumer's latest readIndex_ to know if there is
        // room, hence an acquire load of readIndex_.
        const std::size_t w    = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t next = (w + 1) & kMask;
        if (next == readIndex_.load(std::memory_order_acquire))
            return false; // full

        buffer_[w] = item;
        // release: publish the written slot *before* the consumer can observe the
        // advanced index - this is what makes the data visible across threads.
        writeIndex_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer side. Returns false (and leaves `out` untouched) if empty.
    bool pop(T& out) noexcept
    {
        const std::size_t r = readIndex_.load(std::memory_order_relaxed);
        // acquire: pair with the producer's release so we actually see the item
        // it wrote, not stale memory.
        if (r == writeIndex_.load(std::memory_order_acquire))
            return false; // empty

        out = buffer_[r];
        readIndex_.store((r + 1) & kMask, std::memory_order_release);
        return true;
    }

    bool empty() const noexcept
    {
        return readIndex_.load(std::memory_order_acquire)
               == writeIndex_.load(std::memory_order_acquire);
    }

    // Maximum number of items that can be queued at once.
    static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

private:
    std::array<T, Capacity> buffer_{};
    std::atomic<std::size_t> writeIndex_{0}; // owned by the producer
    std::atomic<std::size_t> readIndex_{0};  // owned by the consumer
    static constexpr std::size_t kMask = Capacity - 1;
};

} // namespace gridseq
