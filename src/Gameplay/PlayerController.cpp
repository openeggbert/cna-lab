#include "IronShadows/Gameplay/PlayerController.hpp"

#include "IronShadows/World/PrototypeWorld.hpp"

#include <numbers>

namespace IronShadows
{
    void PlayerController::Reset(const Vector3& spawnPosition, float yaw)
    {
        position_ = spawnPosition;
        yaw_ = yaw;
    }

    void PlayerController::Update(float deltaSeconds,
                                  const OnFootInput& input,
                                  const PrototypeWorld& world)
    {
        yaw_ += input.turn * turnSpeed_ * deltaSeconds;
        if (yaw_ > std::numbers::pi_v<float>)
        {
            yaw_ -= std::numbers::pi_v<float> * 2.0F;
        }
        else if (yaw_ < -std::numbers::pi_v<float>)
        {
            yaw_ += std::numbers::pi_v<float> * 2.0F;
        }

        const float speed = walkSpeed_ * (input.sprint ? sprintMultiplier_ : 1.0F);
        Vector3 direction = GetForward() * input.forward + RightFromYaw(yaw_) * input.strafe;
        if (direction.LengthSquared() > 1.0F)
        {
            direction.Normalize();
        }

        const Vector3 delta = direction * speed * deltaSeconds;
        position_ = world.ResolveHorizontalMotion(position_, delta, collisionRadius_);
        position_.Y = 1.70F;
    }
}
