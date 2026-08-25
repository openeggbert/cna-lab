#pragma once

#include "IronGang/Core/WorldTypes.hpp"

namespace IronGang
{
    // "Nothing is in the way": a distance no mover in this prototype can be looking that far
    // ahead, so callers can `std::min` over candidates without a found/not-found flag.
    inline constexpr float kNoObstacleAhead = 1000.0F;

    // Half-widths of the two kinds of lane this game has. A traffic lane is wide because a car
    // beside you in the next lane must not brake you; a walking lane is narrow because two
    // pedestrians passing shoulder to shoulder should not stop each other.
    inline constexpr float kTrafficLaneHalfWidth = 2.0F;
    inline constexpr float kWalkingLaneHalfWidth = 0.55F;

    // How far ahead @p obstaclePosition sits for a mover at @p fromPosition facing @p fromYaw, or
    // kNoObstacleAhead when it is behind or further than @p laneHalfWidth to either side. Y is
    // ignored: both roads and sidewalks are flat here, and a pedestrian on a first-floor balcony
    // is not something this prototype has.
    //
    // Shared by traffic (plan_21 IG-21-002's following distance) and pedestrians (plan_20
    // IG-20-010's congestion avoidance) rather than written twice: they differ only in lane width.
    [[nodiscard]] float DistanceAheadInLane(const Vector3& fromPosition,
                                            float fromYaw,
                                            const Vector3& obstaclePosition,
                                            float laneHalfWidth);
}
