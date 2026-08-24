#pragma once

#include <cstdint>

namespace CopperBoots
{
    class SimulationClock
    {
    public:
        static constexpr double TickSeconds = 1.0 / 60.0;
        static constexpr double MaximumFrameSeconds = 0.250;
        static constexpr int MaximumStepsPerFrame = 8;

        [[nodiscard]] int AddFrameTime(double elapsedSeconds);
        void MarkStep() noexcept { ++tick_; }

        [[nodiscard]] std::uint64_t Tick() const noexcept { return tick_; }
        [[nodiscard]] std::uint64_t DroppedSteps() const noexcept
        {
            return droppedSteps_;
        }
        [[nodiscard]] double RemainderSeconds() const noexcept
        {
            return accumulator_;
        }

    private:
        double accumulator_ = 0.0;
        std::uint64_t tick_ = 0;
        std::uint64_t droppedSteps_ = 0;
    };
}

