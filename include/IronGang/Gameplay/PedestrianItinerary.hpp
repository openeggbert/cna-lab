#pragma once

#include "IronGang/Core/RandomSource.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_20 IG-20-002/005: what an ambient pedestrian is doing, and where it is going next.
    //
    // Until now every pedestrian paced one fixed stretch of pavement forever. A city of people who
    // only ever pace is not a city, and it also meant the sidewalk graph's crossings and doorways
    // -- and the router built for them -- had nothing walking through them.
    enum class PedestrianActivity
    {
        // Between destinations.
        Walking,
        // Arrived, and standing there for a while before choosing somewhere else. This is the
        // "idle point" plan_20 IG-20-002 asks for: not a separate kind of node, just what a
        // pedestrian does when it gets where it was going.
        Waiting,
    };

    // How long a pedestrian lingers on arrival, before and after jitter. Long enough to read as
    // someone stopping, short enough that the pavement does not silt up with statues.
    inline constexpr float kPedestrianWaitMinimumSeconds = 2.0F;
    inline constexpr float kPedestrianWaitMaximumSeconds = 8.0F;

    class PedestrianItinerary final
    {
    public:
        // Starts a walk to @p destinationNodeId.
        void BeginWalk(std::string destinationNodeId);
        // Called on arrival: stand here for @p waitSeconds before wanting somewhere new.
        void BeginWait(float waitSeconds) noexcept;
        void Update(float deltaSeconds) noexcept;

        [[nodiscard]] PedestrianActivity GetActivity() const noexcept { return activity_; }
        [[nodiscard]] const std::string& GetDestinationNodeId() const noexcept { return destinationNodeId_; }
        [[nodiscard]] float GetWaitSecondsRemaining() const noexcept { return waitSecondsRemaining_; }
        // True when the pedestrian has nowhere to be: it has never been given a destination, or it
        // has arrived and finished standing about.
        [[nodiscard]] bool WantsNewDestination() const noexcept;

    private:
        PedestrianActivity activity_{PedestrianActivity::Waiting};
        std::string destinationNodeId_;
        float waitSecondsRemaining_{0.0F};
    };

    // Picks somewhere to go: any candidate except where the pedestrian already is. Deterministic
    // given @p random, so an ambient population is reproducible run to run -- which is what makes
    // a profiling capture or a bug report comparable at all. Returns an empty string when there is
    // nowhere else to go, which the caller must treat as "stay put" rather than as an error.
    [[nodiscard]] std::string ChoosePedestrianDestination(const std::vector<std::string>& candidates,
                                                          const std::string& currentNodeId,
                                                          RandomSource& random);
}
