#include "gridseq/Pattern.h"

#include <algorithm>

namespace gridseq {

void Pattern::clear() noexcept
{
    for (auto& row : cells_)
        row.fill(0.0f);
}

bool Pattern::inRange(int track, int step) noexcept
{
    return track >= 0 && track < kMaxTracks && step >= 0 && step < kMaxSteps;
}

void Pattern::setNumTracks(int tracks) noexcept
{
    numTracks_ = std::clamp(tracks, 1, kMaxTracks);
}

void Pattern::setNumSteps(int steps) noexcept
{
    numSteps_ = std::clamp(steps, 1, kMaxSteps);
}

void Pattern::setCell(int track, int step, float velocity) noexcept
{
    if (! inRange(track, step))
        return;
    cells_[static_cast<std::size_t>(track)][static_cast<std::size_t>(step)]
        = std::clamp(velocity, 0.0f, 1.0f);
}

float Pattern::cell(int track, int step) const noexcept
{
    if (! inRange(track, step))
        return 0.0f;
    return cells_[static_cast<std::size_t>(track)][static_cast<std::size_t>(step)];
}

void Pattern::toggle(int track, int step, float onVelocity) noexcept
{
    setCell(track, step, isActive(track, step) ? 0.0f : onVelocity);
}

} // namespace gridseq
