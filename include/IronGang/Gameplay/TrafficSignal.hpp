#pragma once

#include "IronGang/Core/WorldTypes.hpp"

namespace IronGang
{
    // plan_21 IG-21-003/007: a fixed-time traffic light. Mafia-1 fidelity means a light that
    // cycles on a timer, not one that senses traffic or coordinates with its neighbours.
    enum class SignalPhase
    {
        Green,
        Amber,
        Red,
    };

    [[nodiscard]] const char* SignalPhaseName(SignalPhase phase) noexcept;

    struct TrafficSignalTiming
    {
        float greenSeconds{9.0F};
        float amberSeconds{2.0F};
        float redSeconds{11.0F};

        [[nodiscard]] float CycleSeconds() const noexcept
        {
            return greenSeconds + amberSeconds + redSeconds;
        }
    };

    // Drives one light's phase from a clock. Deliberately free of geometry and vehicles: where a
    // light stands and who obeys it are the world's and the game's business, and keeping them out
    // is what makes the timing testable on its own.
    class TrafficSignal final
    {
    public:
        // A non-positive duration in @p timing is ignored, since a phase of zero length is a light
        // that skips a colour rather than a configuration anyone means.
        void Configure(const TrafficSignalTiming& timing) noexcept;
        [[nodiscard]] const TrafficSignalTiming& GetTiming() const noexcept { return timing_; }

        // offsetSeconds starts the light partway through its cycle, which is how two lights at one
        // crossing are made to complement rather than mirror each other.
        void Reset(float offsetSeconds = 0.0F) noexcept;
        void Update(float deltaSeconds) noexcept;

        [[nodiscard]] SignalPhase GetPhase() const noexcept;
        // The phase this light's opposite would be showing: red while this one is green or amber,
        // green while this one is red. One timer, two directions, and no way for them to drift
        // apart -- two independent lights at one crossing eventually show green together.
        [[nodiscard]] SignalPhase GetOpposingPhase() const noexcept;
        [[nodiscard]] float GetSecondsIntoCycle() const noexcept { return elapsedSeconds_; }

        // Whether a vehicle approaching this light should stop. Amber counts: at this scale a car
        // deciding whether it can "make it" is a rule nobody watching would notice, and one that
        // puts vehicles in the intersection when the phase flips.
        [[nodiscard]] static bool RequiresStop(SignalPhase phase) noexcept
        {
            return phase != SignalPhase::Green;
        }

    private:
        TrafficSignalTiming timing_;
        float elapsedSeconds_{0.0F};
    };

    // Where a light stands and which approach it governs (plan_21 IG-21-003). The stop line is a
    // point on the road plus the heading of the traffic it stops, so the same in-lane test traffic
    // already uses for obstacles decides whether a given vehicle is approaching it.
    struct TrafficStopLine
    {
        Vector3 position{};
        // Yaw of vehicles this line stops, in ForwardFromYaw's convention.
        float approachYaw{0.0F};
        // Where the light itself is drawn -- beside the road, not on the stop line.
        Vector3 signalPosition{};
        // True for the second of a crossing's two directions, which reads the opposing phase.
        bool opposingPhase{false};
    };
}
