#include "IronGang/Core/SimulationClock.hpp"

#include <cmath>

namespace IronGang
{
    void SimulationClock::Configure(float maximumStepSeconds) noexcept
    {
        if (maximumStepSeconds > 0.0F && std::isfinite(maximumStepSeconds))
        {
            maximumStepSeconds_ = maximumStepSeconds;
        }
    }

    float SimulationClock::Advance(float rawDeltaSeconds) noexcept
    {
        ++frames_;
        if (!std::isfinite(rawDeltaSeconds) || rawDeltaSeconds <= 0.0F)
        {
            // A NaN, an infinity, or a backwards step is a broken timer, not a request to move
            // the world. The frame still counts: it happened, it simply advanced nothing.
            return 0.0F;
        }
        if (rawDeltaSeconds > maximumStepSeconds_)
        {
            droppedSeconds_ += static_cast<double>(rawDeltaSeconds) - maximumStepSeconds_;
            ++clampedSteps_;
            elapsedSeconds_ += maximumStepSeconds_;
            return maximumStepSeconds_;
        }
        elapsedSeconds_ += rawDeltaSeconds;
        return rawDeltaSeconds;
    }

    void SimulationClock::Reset() noexcept
    {
        elapsedSeconds_ = 0.0;
        droppedSeconds_ = 0.0;
        clampedSteps_ = 0;
        frames_ = 0;
    }
}
