#include "IronGang/Gameplay/PlayerController.hpp"

#include "IronGang/Physics/PhysicsWorld.hpp"

#include <numbers>

namespace IronGang
{
    void PlayerController::Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics)
    {
        // A teleport must not carry momentum into wherever the character lands.
        locomotion_.Stop();
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
        locomotion_.Stop();
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
        // plan_16 IG-16-005: input asks for a speed and a turn rate; locomotion decides how fast
        // the character actually gets there, so starting and stopping are movements rather than
        // switches.
        locomotion_.Update(deltaSeconds, input.forward, input.strafe, input.turn, input.sprint);

        yaw_ += locomotion_.GetTurnRate() * deltaSeconds;
        if (yaw_ > std::numbers::pi_v<float>)
        {
            yaw_ -= std::numbers::pi_v<float> * 2.0F;
        }
        else if (yaw_ < -std::numbers::pi_v<float>)
        {
            yaw_ += std::numbers::pi_v<float> * 2.0F;
        }

        const Vector3 velocity = GetForward() * locomotion_.GetForwardVelocity() +
                                 RightFromYaw(yaw_) * locomotion_.GetStrafeVelocity();

        // CharacterVirtual::Update() does not accumulate gravity into the velocity it is given
        // (see PhysicsWorld.cpp's Step()); a small constant downward bias is enough to keep the
        // capsule grounded and correctly collision-resolved since this prototype has no jump
        // input to require a real accumulated fall speed.
        const Vector3 desiredVelocity = velocity + Vector3(0.0F, -4.0F, 0.0F);
        physics.MoveCharacter(characterHandle_, desiredVelocity, deltaSeconds);

        // Only one of PlayerController::Update()/VehicleController::Update() runs per game frame
        // (IronGangGame::Update() branches on playerDriving_), so it is safe -- and required,
        // since nothing else does -- for each to step the shared PhysicsWorld exactly once here.
        physics.Step(deltaSeconds);

        position_ = physics.GetCharacterPosition(characterHandle_);
        grounded_ = physics.IsCharacterGrounded(characterHandle_);
    }
}
