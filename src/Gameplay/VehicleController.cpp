#include "IronShadows/Gameplay/VehicleController.hpp"

#include "IronShadows/World/PrototypeWorld.hpp"

#include <algorithm>
#include <cmath>

namespace IronShadows
{
    void VehicleController::Reset(const Vector3& spawnPosition, float yaw)
    {
        position_ = spawnPosition;
        yaw_ = yaw;
        speed_ = 0.0F;
    }

    void VehicleController::Restore(const Vector3& position, float yaw, float speed)
    {
        position_ = position;
        yaw_ = yaw;
        speed_ = std::clamp(speed, -maxReverseSpeed_, maxForwardSpeed_);
    }

    void VehicleController::Update(float deltaSeconds,
                                   const VehicleInput& input,
                                   const PrototypeWorld& world)
    {
        const float throttle = std::clamp(input.throttle, -1.0F, 1.0F);
        if (throttle > 0.0F)
        {
            speed_ += throttle * engineAcceleration_ * deltaSeconds;
        }
        else if (throttle < 0.0F)
        {
            speed_ += throttle * reverseAcceleration_ * deltaSeconds;
        }
        else
        {
            const float drag = rollingDrag_ + aerodynamicDrag_ * speed_ * speed_;
            const float drop = drag * deltaSeconds;
            if (std::abs(speed_) <= drop)
            {
                speed_ = 0.0F;
            }
            else
            {
                speed_ -= std::copysign(drop, speed_);
            }
        }

        if (input.handbrake)
        {
            speed_ *= std::max(0.0F, 1.0F - 5.0F * deltaSeconds);
        }

        speed_ = std::clamp(speed_, -maxReverseSpeed_, maxForwardSpeed_);

        const float speedFactor = std::clamp(std::abs(speed_) / 8.0F, 0.15F, 1.0F);
        const float directionSign = speed_ >= 0.0F ? 1.0F : -1.0F;
        yaw_ += std::clamp(input.steering, -1.0F, 1.0F) * steeringRate_ * speedFactor *
                directionSign * deltaSeconds;

        const Vector3 attemptedDelta = GetForward() * speed_ * deltaSeconds;
        const Vector3 oldPosition = position_;
        position_ = world.ResolveHorizontalMotion(position_, attemptedDelta, collisionRadius_);
        position_.Y = 0.65F;

        const float requestedDistanceSq = attemptedDelta.X * attemptedDelta.X + attemptedDelta.Z * attemptedDelta.Z;
        const float movedX = position_.X - oldPosition.X;
        const float movedZ = position_.Z - oldPosition.Z;
        const float movedDistanceSq = movedX * movedX + movedZ * movedZ;
        if (requestedDistanceSq > 0.0001F && movedDistanceSq < requestedDistanceSq * 0.25F)
        {
            speed_ *= -0.15F;
        }
    }
}
