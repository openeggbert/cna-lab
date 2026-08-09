#include "IronGang/Gameplay/VehicleController.hpp"

#include "IronGang/Physics/PhysicsWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace IronGang
{
    void VehicleController::EnsureCreated(const Vector3& position, Physics::PhysicsWorld& physics)
    {
        if (vehicleHandle_.IsValid())
        {
            return;
        }

        // Matches PrototypeRenderer's existing procedural/CNJ body box (2.1 x 0.65 x 4.2) and
        // per-wheel local offsets, so the physics chassis lines up with the rendered sedan.
        const Vector3 chassisHalfExtents(1.05F, 0.325F, 2.1F);
        const std::array<Vector3, 4> wheelLocalPositions = {
            Vector3{-1.05F, -0.20F, -1.35F}, Vector3{1.05F, -0.20F, -1.35F},
            Vector3{-1.05F, -0.20F, 1.35F},  Vector3{1.05F, -0.20F, 1.35F},
        };

        vehicleHandle_ = physics.CreateFourWheelVehicle(
            chassisHalfExtents, kChassisMass, position, wheelLocalPositions, kWheelRadius, kWheelWidth);
    }

    void VehicleController::Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics)
    {
        EnsureCreated(spawnPosition, physics);
        physics.SetVehicleTransform(vehicleHandle_, spawnPosition, yaw, 0.0F);
        position_ = spawnPosition;
        yaw_ = yaw;
        speed_ = 0.0F;
        previousForwardInput_ = 0.0F;
    }

    void VehicleController::Restore(const Vector3& position, float yaw, float speed, Physics::PhysicsWorld& physics)
    {
        EnsureCreated(position, physics);
        const float clampedSpeed = std::clamp(speed, -maxReverseSpeed_, maxForwardSpeed_);
        physics.SetVehicleTransform(vehicleHandle_, position, yaw, clampedSpeed);
        position_ = position;
        yaw_ = yaw;
        speed_ = clampedSpeed;
        previousForwardInput_ = 0.0F;
    }

    void VehicleController::Update(float deltaSeconds,
                                   const VehicleInput& input,
                                   Physics::PhysicsWorld& physics)
    {
        // Mirrors Jolt's own VehicleConstraintTest sample: only brake when the driver is trying
        // to reverse direction while still moving the old way; otherwise forward input alone is
        // enough (engine/transmission/rolling friction handle deceleration when it drops to 0).
        float forward = std::clamp(input.throttle, -1.0F, 1.0F);
        float brake = 0.0F;
        if (previousForwardInput_ * forward < 0.0F)
        {
            if ((forward > 0.0F && speed_ < -0.1F) || (forward < 0.0F && speed_ > 0.1F))
            {
                forward = 0.0F;
                brake = 1.0F;
            }
            else
            {
                previousForwardInput_ = forward;
            }
        }
        else if (forward != 0.0F)
        {
            previousForwardInput_ = forward;
        }

        const float steer = std::clamp(input.steering, -1.0F, 1.0F);
        const float handBrake = input.handbrake ? 1.0F : 0.0F;
        physics.SetVehicleInput(vehicleHandle_, forward, steer, brake, handBrake);

        physics.Step(deltaSeconds);

        position_ = physics.GetVehiclePosition(vehicleHandle_);
        yaw_ = physics.GetVehicleYaw(vehicleHandle_);

        const Vector3 velocity = physics.GetVehicleLinearVelocity(vehicleHandle_);
        speed_ = Vector3::Dot(velocity, GetForward());
    }
}
