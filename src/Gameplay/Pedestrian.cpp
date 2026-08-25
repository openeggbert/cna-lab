#include "IronGang/Gameplay/Pedestrian.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
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

    void Pedestrian::Update(float deltaSeconds, bool hasThreat, const Vector3& threatPosition)
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

        yaw_ = AdvanceAlongPath(path_, position_, targetIndex_, walkSpeed_, deltaSeconds, kArrivalRadius, yaw_);
    }
}
