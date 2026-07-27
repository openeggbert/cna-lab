#pragma once

#include "IronShadows/Core/WorldTypes.hpp"

#include <cstddef>
#include <vector>

namespace IronShadows
{
    // Gate M9 (plan_19-navigation-and-pathfinding.md IS-19-001/002): the smallest possible
    // navigation representation -- a hand-authored, ordered list of world-space points a mover
    // walks/drives between in sequence. Deliberately not a general graph (no branching, no
    // per-edge costs, no pathfinding search): Mafia-1-fidelity traffic/pedestrians only need to
    // follow one fixed loop each, not choose between routes.
    struct WaypointPath
    {
        std::vector<Vector3> points;
        // Whether reaching the last point wraps back to the first (a closed loop) rather than
        // stopping there. A 2-point non-loop path is a dead end; a 2-point loop path is a
        // back-and-forth (used for sidewalks, see PrototypeWorld::GetSidewalkPaths()).
        bool loop{true};

        [[nodiscard]] bool Empty() const noexcept { return points.empty(); }
    };

    // Moves `position` toward path.points[targetIndex] at `speed` units/second for
    // `deltaSeconds`, advancing (and wrapping, if path.loop) targetIndex once within
    // `arrivalRadius` of the current target. Returns the yaw (matching ForwardFromYaw's own
    // convention) the mover should now face -- the direction of travel this frame -- or
    // `previousYaw` unchanged if it didn't move (an empty path, non-positive speed, or already
    // exactly at its target). Shared by TrafficVehicle and Pedestrian rather than duplicated:
    // both are "follow this fixed loop" movers differing only in speed/braking/flee behavior,
    // which stays in each of their own Update() methods.
    [[nodiscard]] float AdvanceAlongPath(const WaypointPath& path,
                                        Vector3& position,
                                        std::size_t& targetIndex,
                                        float speed,
                                        float deltaSeconds,
                                        float arrivalRadius,
                                        float previousYaw);
}
