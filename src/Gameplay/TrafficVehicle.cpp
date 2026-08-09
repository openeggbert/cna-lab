#include "IronGang/Gameplay/TrafficVehicle.hpp"

#include <algorithm>

namespace IronGang
{
    namespace
    {
        constexpr float kBrakingDistance = 10.0F;
        constexpr float kMinGap = 3.0F;
        constexpr float kAccelerationPerSecond = 6.0F;
        constexpr float kBrakingPerSecond = 12.0F;
        constexpr float kArrivalRadius = 1.0F;
    }

    void TrafficVehicle::Reset(WaypointPath path, std::size_t startIndex, float cruiseSpeed)
    {
        path_ = std::move(path);
        targetIndex_ = path_.Empty() ? 0 : (startIndex % path_.points.size());
        position_ = path_.Empty() ? Vector3{} : path_.points[targetIndex_];
        cruiseSpeed_ = cruiseSpeed;
        currentSpeed_ = 0.0F;
        yaw_ = 0.0F;
    }

    void TrafficVehicle::Update(float deltaSeconds, float obstacleDistanceAhead)
    {
        float targetSpeed = cruiseSpeed_;
        if (obstacleDistanceAhead < kBrakingDistance)
        {
            const float t = std::clamp((obstacleDistanceAhead - kMinGap) / (kBrakingDistance - kMinGap), 0.0F, 1.0F);
            targetSpeed = cruiseSpeed_ * t;
        }

        if (currentSpeed_ < targetSpeed)
        {
            currentSpeed_ = std::min(targetSpeed, currentSpeed_ + kAccelerationPerSecond * deltaSeconds);
        }
        else
        {
            currentSpeed_ = std::max(targetSpeed, currentSpeed_ - kBrakingPerSecond * deltaSeconds);
        }

        yaw_ = AdvanceAlongPath(path_, position_, targetIndex_, currentSpeed_, deltaSeconds, kArrivalRadius, yaw_);
    }
}
