#include "CopperBoots/SimulationClock.hpp"

#include <algorithm>
#include <cmath>

namespace CopperBoots
{
    int SimulationClock::AddFrameTime(const double elapsedSeconds)
    {
        accumulator_ += std::clamp(elapsedSeconds, 0.0, MaximumFrameSeconds);
        const int available = static_cast<int>(
            std::floor((accumulator_ + 1.0e-12) / TickSeconds));
        const int steps = std::min(available, MaximumStepsPerFrame);
        accumulator_ -= static_cast<double>(steps) * TickSeconds;

        if (available > MaximumStepsPerFrame) {
            const int dropped = available - MaximumStepsPerFrame;
            droppedSteps_ += static_cast<std::uint64_t>(dropped);
            accumulator_ = std::fmod(accumulator_, TickSeconds);
        }
        return steps;
    }
}

