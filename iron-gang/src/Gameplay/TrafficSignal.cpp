#include "IronGang/Gameplay/TrafficSignal.hpp"

#include <cmath>

namespace IronGang
{
    const char* SignalPhaseName(SignalPhase phase) noexcept
    {
        switch (phase)
        {
            case SignalPhase::Green: return "green";
            case SignalPhase::Amber: return "amber";
            case SignalPhase::Red: return "red";
        }
        return "red";
    }

    void TrafficSignal::Configure(const TrafficSignalTiming& timing) noexcept
    {
        if (timing.greenSeconds > 0.0F && timing.amberSeconds > 0.0F && timing.redSeconds > 0.0F &&
            std::isfinite(timing.CycleSeconds()))
        {
            timing_ = timing;
        }
    }

    void TrafficSignal::Reset(float offsetSeconds) noexcept
    {
        const float cycle = timing_.CycleSeconds();
        if (!std::isfinite(offsetSeconds) || cycle <= 0.0F)
        {
            elapsedSeconds_ = 0.0F;
            return;
        }
        elapsedSeconds_ = std::fmod(std::fmax(offsetSeconds, 0.0F), cycle);
    }

    void TrafficSignal::Update(float deltaSeconds) noexcept
    {
        if (!(deltaSeconds > 0.0F) || !std::isfinite(deltaSeconds))
        {
            return;
        }
        const float cycle = timing_.CycleSeconds();
        elapsedSeconds_ = std::fmod(elapsedSeconds_ + deltaSeconds, cycle);
    }

    SignalPhase TrafficSignal::GetPhase() const noexcept
    {
        if (elapsedSeconds_ < timing_.greenSeconds)
        {
            return SignalPhase::Green;
        }
        if (elapsedSeconds_ < timing_.greenSeconds + timing_.amberSeconds)
        {
            return SignalPhase::Amber;
        }
        return SignalPhase::Red;
    }

    SignalPhase TrafficSignal::GetOpposingPhase() const noexcept
    {
        // Green only while this direction is fully stopped -- and not during this direction's
        // amber, or both directions would be moving through the crossing at once.
        switch (GetPhase())
        {
            case SignalPhase::Green:
            case SignalPhase::Amber:
                return SignalPhase::Red;
            case SignalPhase::Red:
                break;
        }
        // The opposing direction gets its own amber at the end of this direction's red, so it is
        // stopped again before this one turns green.
        const float redElapsed = elapsedSeconds_ - (timing_.greenSeconds + timing_.amberSeconds);
        return redElapsed >= timing_.redSeconds - timing_.amberSeconds ? SignalPhase::Amber
                                                                       : SignalPhase::Green;
    }
}
