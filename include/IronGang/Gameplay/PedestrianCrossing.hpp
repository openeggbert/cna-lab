#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Gameplay/LaneClearance.hpp"
#include "IronGang/Gameplay/TrafficSignal.hpp"

#include <vector>

namespace IronGang
{
    // plan_20 IG-20-012: pedestrians cross at the marked crossing, and wait for the signal.
    //
    // Reuses the mechanism already there rather than adding a state machine: a pedestrian who may
    // not step off the kerb is a pedestrian with an obstacle right in front of them, and
    // `Pedestrian::Update()` already slows and stops for `clearanceAheadMetres`. Traffic vehicles
    // treat a red light exactly the same way (plan_21 IG-21-007) -- one idea, used twice.

    // A pedestrian may cross a signalled crossing precisely when the traffic it would walk in
    // front of has been stopped. Amber counts as stopped for vehicles (TrafficSignal's own
    // RequiresStop()), but **not** as safe to start walking: a car already committed to the
    // junction is still coming through.
    [[nodiscard]] bool PedestrianMayCross(SignalPhase trafficPhase, bool signalControlled) noexcept;

    // How close to a kerb a pedestrian has to be for the kerb to hold them. Someone already in the
    // road is past holding: the rule has to let them finish, or a signal changing mid-crossing
    // freezes them in a live lane.
    inline constexpr float kKerbHoldRadiusMetres = 1.0F;

    // A crossing, as the pedestrian rule needs it: the two kerbs, and whether a signal governs it.
    struct CrossingPair
    {
        Vector3 kerbA{};
        Vector3 kerbB{};
        bool signalControlled{true};
    };

    // The clearance to pass to Pedestrian::Update(): zero when a kerb is holding this pedestrian,
    // and unobstructed otherwise.
    //
    // Holding requires **both** that they are standing at one kerb and that the waypoint they are
    // walking toward is the other one. Position alone is not enough once every pedestrian routes
    // freely: someone walking *along* the pavement passes within a metre of the kerb constantly,
    // and stopping them dead every time the light is green would jam the pavement rather than the
    // crossing.
    [[nodiscard]] float PedestrianCrossingClearance(const Vector3& position,
                                                    const Vector3& targetPoint,
                                                    const std::vector<CrossingPair>& crossings,
                                                    SignalPhase trafficPhase) noexcept;
}
