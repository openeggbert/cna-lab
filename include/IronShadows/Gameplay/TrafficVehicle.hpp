#pragma once

#include "IronShadows/World/WaypointPath.hpp"

#include <cstddef>

namespace IronShadows
{
    // Gate M9 (plan_21-traffic-simulation.md IS-21-001/002/004): a single traffic vehicle
    // following a fixed WaypointPath loop at a cruise speed, braking smoothly when something is
    // ahead of it (the player's vehicle or another TrafficVehicle -- callers compute this
    // externally, see IronShadowsGame's traffic tick, and pass in just the resulting distance).
    // Deliberately a kinematic mover, not a Jolt rigid body: it only ever needs "follow this
    // path, slow down for what's ahead", which real rigid-body dynamics would not improve for
    // this prototype's fidelity target and would add real physics-integration risk for no
    // benefit. Signals/stop-lines are explicitly out of scope for this first pass (the single
    // intersection in WarehouseBlock has no light in the existing geometry) -- see
    // plan_21-traffic-simulation.md IS-21-003.
    class TrafficVehicle final
    {
    public:
        void Reset(WaypointPath path, std::size_t startIndex, float cruiseSpeed);

        // obstacleDistanceAhead: distance to the nearest thing directly ahead of this vehicle
        // along its current facing direction, or a large sentinel (no practical upper bound
        // needed -- anything past the braking distance behaves identically) when nothing is
        // close enough to matter.
        void Update(float deltaSeconds, float obstacleDistanceAhead);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] float GetForwardSpeed() const noexcept { return currentSpeed_; }

    private:
        WaypointPath path_;
        std::size_t targetIndex_{0};
        Vector3 position_{};
        float yaw_{0.0F};
        float cruiseSpeed_{6.0F};
        float currentSpeed_{0.0F};
    };
}
