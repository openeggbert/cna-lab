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

        // Shortest signed angle from `from` to `to`, in (-pi, pi].
        [[nodiscard]] float ShortestAngleTo(float from, float to) noexcept
        {
            constexpr float kPi = 3.14159265358979323846F;
            float delta = to - from;
            while (delta > kPi)
            {
                delta -= 2.0F * kPi;
            }
            while (delta <= -kPi)
            {
                delta += 2.0F * kPi;
            }
            return delta;
        }
    }

    void Pedestrian::Reset(WaypointPath path, std::size_t startIndex, float walkSpeed,
                           float startOffsetMetres)
    {
        path_ = std::move(path);
        targetIndex_ = path_.Empty() ? 0 : (startIndex % path_.points.size());
        position_ = path_.Empty() ? Vector3{} : path_.points[targetIndex_];
        walkSpeed_ = walkSpeed;
        yaw_ = 0.0F;
        walking_ = false;
        turningInPlace_ = false;
        turnRate_ = 0.0F;
        fleeTimer_ = 0.0F;
        fleeFromPosition_ = Vector3{};

        if (path_.points.size() < 2)
        {
            return;
        }

        // plan_20 IG-20-003: face the way you are about to walk. Yaw used to be left at 0 unless a
        // start offset was given, which was invisible while turning was instantaneous -- now it
        // would make every pedestrian pivot on the spot the moment it spawned.
        {
            const std::size_t firstTarget = (targetIndex_ + 1) % path_.points.size();
            Vector3 heading = path_.points[firstTarget] - position_;
            heading.Y = 0.0F;
            if (heading.Length() > 1e-4F)
            {
                yaw_ = std::atan2(heading.X, -heading.Z);
            }
        }

        if (startOffsetMetres <= 0.0F)
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
            walking_ = true;
            // Panic is not a considered pivot: a fleeing pedestrian faces away immediately and
            // keeps running. The rate limit below deliberately does not apply here.
            turningInPlace_ = false;
            turnRate_ = 0.0F;
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
        // "Walking" means moving visibly, not moving at all: a pedestrian creeping the last few
        // centimetres up to the person ahead should read as standing, or its walk animation slides
        // along the pavement at a speed nobody's legs are matching. Same threshold as Locomotion's
        // IsMoving() for the same reason.
        walking_ = speed > 0.05F;
        if (speed <= 0.0F)
        {
            // Stopped, but still facing the way it was going: a queue of people all facing
            // forward, not a huddle.
            turningInPlace_ = false;
            turnRate_ = 0.0F;
            return;
        }

        // plan_20 IG-20-003: turn toward the heading the path wants at a bounded rate rather than
        // snapping to it. AdvanceAlongPath() returns the exact heading of the current segment,
        // which at the end of a two-point pavement is a 180-degree reversal -- applied directly,
        // that is a pedestrian who spins in one frame.
        const float previousYaw = yaw_;
        const float desiredYaw = HeadingTowardTarget();
        const float error = ShortestAngleTo(yaw_, desiredYaw);
        const float maximumStep = kPedestrianTurnRate * deltaSeconds;
        turningInPlace_ = std::abs(error) > kPedestrianTurnInPlaceThreshold;
        yaw_ += std::clamp(error, -maximumStep, maximumStep);
        turnRate_ = deltaSeconds > 0.0F ? ShortestAngleTo(previousYaw, yaw_) / deltaSeconds : 0.0F;

        if (turningInPlace_)
        {
            // Pivoting on the spot: a person at the end of a pavement turns round before walking
            // back, rather than walking backwards while rotating.
            walking_ = false;
            return;
        }

        // AdvanceAlongPath() moves the pedestrian and advances targetIndex_; its returned heading
        // is discarded in favour of the rate-limited one computed above.
        (void)AdvanceAlongPath(path_, position_, targetIndex_, speed, deltaSeconds, kArrivalRadius, yaw_);
    }

    bool Pedestrian::HasArrived() const noexcept
    {
        if (path_.loop || path_.points.size() < 2)
        {
            return false;
        }
        if (targetIndex_ + 1 != path_.points.size())
        {
            return false;
        }
        Vector3 remaining = path_.points[targetIndex_] - position_;
        remaining.Y = 0.0F;
        return remaining.Length() <= kArrivalRadius;
    }

    Vector3 Pedestrian::GetTargetPoint() const noexcept
    {
        if (path_.points.empty())
        {
            return position_;
        }
        return path_.points[targetIndex_ % path_.points.size()];
    }

    float Pedestrian::HeadingTowardTarget() const noexcept
    {
        if (path_.points.empty())
        {
            return yaw_;
        }
        Vector3 toTarget = path_.points[targetIndex_ % path_.points.size()] - position_;
        toTarget.Y = 0.0F;
        if (toTarget.Length() <= 1e-4F)
        {
            return yaw_;
        }
        return std::atan2(toTarget.X, -toTarget.Z);
    }
}
