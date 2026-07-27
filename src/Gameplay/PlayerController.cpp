#include "IronShadows/Gameplay/PlayerController.hpp"

#include "IronShadows/Physics/PhysicsWorld.hpp"

#include <numbers>

namespace IronShadows
{
    void PlayerController::Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics)
    {
        if (!characterHandle_.IsValid())
        {
            characterHandle_ = physics.CreateCharacter(spawnPosition, collisionRadius_, capsuleCylinderHalfHeight_);
        }
        physics.SetCharacterTransform(characterHandle_, spawnPosition, yaw);
        position_ = spawnPosition;
        yaw_ = yaw;
        grounded_ = true;
    }

    void PlayerController::SetPosition(const Vector3& position, Physics::PhysicsWorld& physics)
    {
        position_ = position;
        if (characterHandle_.IsValid())
        {
            physics.SetCharacterTransform(characterHandle_, position, yaw_);
        }
    }

    void PlayerController::SetYaw(float yaw, Physics::PhysicsWorld& physics)
    {
        yaw_ = yaw;
        if (characterHandle_.IsValid())
        {
            physics.SetCharacterTransform(characterHandle_, position_, yaw);
        }
    }

    void PlayerController::Update(float deltaSeconds,
                                  const OnFootInput& input,
                                  Physics::PhysicsWorld& physics)
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

        // CharacterVirtual::Update() does not accumulate gravity into the velocity it is given
        // (see PhysicsWorld.cpp's Step()); a small constant downward bias is enough to keep the
        // capsule grounded and correctly collision-resolved since this prototype has no jump
        // input to require a real accumulated fall speed.
        const Vector3 desiredVelocity = direction * speed + Vector3(0.0F, -4.0F, 0.0F);
        physics.MoveCharacter(characterHandle_, desiredVelocity, deltaSeconds);

        // Only one of PlayerController::Update()/VehicleController::Update() runs per game frame
        // (IronShadowsGame::Update() branches on playerDriving_), so it is safe -- and required,
        // since nothing else does -- for each to step the shared PhysicsWorld exactly once here.
        physics.Step(deltaSeconds);

        position_ = physics.GetCharacterPosition(characterHandle_);
        grounded_ = physics.IsCharacterGrounded(characterHandle_);
    }
}
