#include "IronGang/Gameplay/Pedestrian.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    namespace
    {
        // Walking scale, not driving scale: people close to within an arm's length before they
        // stop, and only slow over the last couple of steps.
        constexpr float kCongestionSlowDistance = 2.0F;
        constexpr float kCongestionStopDistance = 0.7F;
    }

    namespace
    {
        constexpr float kFleeDurationSeconds = 4.0F;
        constexpr float kFleeSpeedMultiplier = 2.5F;
        constexpr float kArrivalRadius = 0.5F;
    }

    void Pedestrian::Reset(WaypointPath path, std::size_t startIndex, float walkSpeed,
                           float startOffsetMetres)
    {
        path_ = std::move(path);
        targetIndex_ = path_.Empty() ? 0 : (startIndex % path_.points.size());
        position_ = path_.Empty() ? Vector3{} : path_.points[targetIndex_];
        walkSpeed_ = walkSpeed;
        yaw_ = 0.0F;
        fleeTimer_ = 0.0F;
        fleeFromPosition_ = Vector3{};

        if (path_.points.size() < 2 || startOffsetMetres <= 0.0F)
        {
            return;
        }

        // Walk the offset along the segment toward the next waypoint and take that waypoint as
        // the target, so a pedestrian spawned mid-segment continues in the direction it would
        // have been walking rather than turning round on its first step.
        const std::size_t nextIndex = (targetIndex_ + 1) % path_.points.size();
        const Vector3 segment = path_.points[nextIndex] - position_;
        const float segmentLength = segment.Length();
        if (segmentLength <= 1e-4F)
        {
            return;
        }
        const float travelled = std::min(startOffsetMetres, segmentLength);
        position_ += (segment / segmentLength) * travelled;
        targetIndex_ = nextIndex;
        yaw_ = std::atan2(segment.X, -segment.Z);
    }

    Vector3 Pedestrian::GetPosition() const noexcept
    {
        if (laneOffsetMetres_ == 0.0F)
        {
            return position_;
        }
        // Right of the direction of travel, in the same yaw convention ForwardFromYaw uses.
        const Vector3 forward = ForwardFromYaw(yaw_);
        const Vector3 right(-forward.Z, 0.0F, forward.X);
        return position_ + right * laneOffsetMetres_;
    }

    void Pedestrian::Update(float deltaSeconds,
                            bool hasThreat,
                            const Vector3& threatPosition,
                            float clearanceAheadMetres)
    {
        if (hasThreat)
        {
            fleeTimer_ = kFleeDurationSeconds;
            fleeFromPosition_ = threatPosition;
        }
        else if (fleeTimer_ > 0.0F)
        {
            fleeTimer_ -= deltaSeconds;
        }

        if (fleeTimer_ > 0.0F)
        {
            Vector3 away = position_ - fleeFromPosition_;
            away.Y = 0.0F;
            const float distance = away.Length();
            if (distance > 1e-4F)
            {
                const Vector3 direction = away / distance;
                const float step = walkSpeed_ * kFleeSpeedMultiplier * deltaSeconds;
                position_ += direction * step;
                yaw_ = std::atan2(direction.X, -direction.Z);
            }
            return;
        }

        // plan_20 IG-20-010: slow down as the pedestrian ahead gets closer, and stop rather than
        // walk through them. The same shape as TrafficVehicle's following distance, at walking
        // scale -- people leave far less room than cars do.
        float speed = walkSpeed_;
        if (clearanceAheadMetres < kCongestionSlowDistance)
        {
            const float t = std::clamp((clearanceAheadMetres - kCongestionStopDistance) /
                                           (kCongestionSlowDistance - kCongestionStopDistance),
                                       0.0F, 1.0F);
            speed = walkSpeed_ * t;
        }
        if (speed <= 0.0F)
        {
            // Stopped, but still facing the way it was going: a queue of people all facing
            // forward, not a huddle.
            return;
        }

        yaw_ = AdvanceAlongPath(path_, position_, targetIndex_, speed, deltaSeconds, kArrivalRadius, yaw_);
    }
}
