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

    // The clearance to pass to Pedestrian::Update() for a pedestrian on a crossing: unobstructed
    // when they may cross or are already out in the road, and zero when the kerb is holding them.
    [[nodiscard]] float PedestrianCrossingClearance(const Vector3& position,
                                                    const std::vector<Vector3>& kerbs,
                                                    bool mayCross) noexcept;
}
