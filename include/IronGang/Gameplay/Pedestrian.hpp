#pragma once

#include "IronGang/Gameplay/LaneClearance.hpp"
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
    // How fast a pedestrian can change heading, radians per second. About 200 degrees a second,
    // so the 180-degree reversal at the end of a pavement takes a bit under a second -- long
    // enough to read as a turn, short enough not to hold up the queue behind them.
    inline constexpr float kPedestrianTurnRate = 3.5F;
    // Heading error above which the pedestrian stops and pivots instead of walking. Below it, the
    // turn happens while walking, which is what a gentle corner looks like.
    inline constexpr float kPedestrianTurnInPlaceThreshold = 0.6F;

    class Pedestrian final
    {
    public:
        // laneOffsetMetres shifts this pedestrian sideways off the path's centreline -- positive
        // is to its right. Six pedestrians sharing one two-point sidewalk would otherwise walk
        // straight through each other; giving the two directions of travel opposite offsets makes
        // them pass side by side instead (plan_20 IG-20-010).
        void SetLaneOffset(float laneOffsetMetres) noexcept { laneOffsetMetres_ = laneOffsetMetres; }
        [[nodiscard]] float GetLaneOffset() const noexcept { return laneOffsetMetres_; }

        // startOffsetMetres moves the pedestrian that far along the segment from
        // path.points[startIndex] toward the next waypoint before it starts walking, clamped to
        // that segment. It exists so several pedestrians can share one sidewalk path without all
        // of them standing on the same endpoint (plan_20 IG-20-001); 0 keeps the original
        // behaviour exactly.
        void Reset(WaypointPath path, std::size_t startIndex, float walkSpeed,
                   float startOffsetMetres = 0.0F);

        // hasThreat/threatPosition describe the single nearest thing this pedestrian should
        // consider fleeing from this frame; pass hasThreat=false once nothing is close (the
        // flee state keeps running off its own timer using the last threat position it saw).
        //
        // clearanceAheadMetres is how far away the nearest pedestrian ahead in this one's walking
        // lane is (kNoObstacleAhead when the way is clear). Walking slows and then stops as that
        // closes, so a queue forms instead of a pile. A **fleeing** pedestrian ignores it on
        // purpose: someone running from a car does not politely queue.
        void Update(float deltaSeconds,
                    bool hasThreat,
                    const Vector3& threatPosition,
                    float clearanceAheadMetres = kNoObstacleAhead);

        // Where the pedestrian actually stands: the point it has walked to along the path, shifted
        // by its lane offset. This is what everything outside sees -- rendering, witness checks,
        // and congestion queries alike -- so a lane offset is not merely cosmetic.
        [[nodiscard]] Vector3 GetPosition() const noexcept;
        // The unshifted point on the path's centreline, which is what path following works in.
        [[nodiscard]] const Vector3& GetPathPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] bool IsFleeing() const noexcept { return fleeTimer_ > 0.0F; }
        // Whether the last Update() actually moved this pedestrian: false while it is stopped
        // behind someone in its lane (plan_20 IG-20-010), which is what lets an animation pick an
        // idle pose instead of sliding a walk cycle along the pavement.
        [[nodiscard]] bool IsWalking() const noexcept { return walking_; }
        // plan_20 IG-20-003: true while the pedestrian is pivoting toward a heading it is not
        // facing yet -- which is what happens at the end of a two-point pavement, where it used to
        // reverse 180 degrees in a single frame. It stops walking for the duration, so a turn is a
        // movement rather than a teleport, and so the animation can play a pivot instead of
        // sliding a walk cycle sideways.
        [[nodiscard]] bool IsTurningInPlace() const noexcept { return turningInPlace_; }
        // How fast the heading is changing, radians per second, signed. Bounded by
        // kPedestrianTurnRate; a caller can use it to pick a left/right pivot once the character
        // model has one.
        [[nodiscard]] float GetTurnRate() const noexcept { return turnRate_; }

    private:
        // The heading the current path target asks for, without moving.
        [[nodiscard]] float HeadingTowardTarget() const noexcept;

        WaypointPath path_;
        std::size_t targetIndex_{0};
        Vector3 position_{};
        float yaw_{0.0F};
        float walkSpeed_{1.6F};
        float fleeTimer_{0.0F};
        float laneOffsetMetres_{0.0F};
        bool walking_{false};
        bool turningInPlace_{false};
        float turnRate_{0.0F};
        Vector3 fleeFromPosition_{};
    };
}
