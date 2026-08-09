#pragma once

#include "IronGang/World/WaypointPath.hpp"

#include <cstddef>

namespace IronGang
{
    // Gate M9 (plan_20-pedestrians-and-ambient-ai.md IG-20-001/002/003): a pedestrian that walks
    // a fixed sidewalk WaypointPath back and forth, overridden by a timed "flee directly away
    // from the threat" state whenever a threat (the player's vehicle, today) comes close enough.
    // No pedestrian-pedestrian avoidance and no panic-scatter-in-all-directions crowd behavior --
    // each pedestrian reacts to the single nearest threat independently, matching the locked
    // "smallest coherent slice" scope for this first pass.
    class Pedestrian final
    {
    public:
        void Reset(WaypointPath path, std::size_t startIndex, float walkSpeed);

        // hasThreat/threatPosition describe the single nearest thing this pedestrian should
        // consider fleeing from this frame; pass hasThreat=false once nothing is close (the
        // flee state keeps running off its own timer using the last threat position it saw).
        void Update(float deltaSeconds, bool hasThreat, const Vector3& threatPosition);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] bool IsFleeing() const noexcept { return fleeTimer_ > 0.0F; }

    private:
        WaypointPath path_;
        std::size_t targetIndex_{0};
        Vector3 position_{};
        float yaw_{0.0F};
        float walkSpeed_{1.6F};
        float fleeTimer_{0.0F};
        Vector3 fleeFromPosition_{};
    };
}
