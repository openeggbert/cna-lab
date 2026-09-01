#include "IronGang/Gameplay/VehicleController.hpp"

#include "IronGang/Core/Log.hpp"

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

        // The defaults in VehicleConfig are the numbers this function used to hard-code; a tuning
        // file replaces them. They must keep matching PrototypeRenderer's body box and per-wheel
        // offsets, or the physics chassis and the rendered sedan stop lining up.
        vehicleHandle_ = physics.CreateFourWheelVehicle(config_.chassisHalfExtents, config_.chassisMass,
                                                        position, config_.wheelPositions,
                                                        config_.wheelRadius, config_.wheelWidth);
    }

    void VehicleController::Configure(const VehicleConfig& config)
    {
        damage_.Configure(config.damage);
        if (vehicleHandle_.IsValid())
        {
            // The chassis, wheels, and mass were baked into the physics body at creation, so this
            // call can only change the speed limits. Saying so beats a tuning file that silently
            // does half of what it says.
            Log::Warning(LogCategory::Assets,
                         "vehicle tuning applied after the body was created; mass and geometry keep "
                         "their previous values");
        }
        config_ = config;
    }

    void VehicleController::Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics)
    {
        EnsureCreated(spawnPosition, physics);
        physics.SetVehicleTransform(vehicleHandle_, spawnPosition, yaw, 0.0F);
        position_ = spawnPosition;
        yaw_ = yaw;
        speed_ = 0.0F;
        previousForwardInput_ = 0.0F;
        // A reset is a fresh start or a retry: the player gets an intact car, not the wreck they
        // arrived in.
        damage_.Reset();
        lastImpactSeverity_ = 0.0F;
    }

    void VehicleController::Restore(const Vector3& position, float yaw, float speed, Physics::PhysicsWorld& physics)
    {
        EnsureCreated(position, physics);
        const float clampedSpeed = std::clamp(speed, -config_.maxReverseSpeed, config_.maxForwardSpeed);
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

        // A wrecked car keeps steering and rolling, but the engine no longer pulls the way it
        // did -- being stranded in a wreck is a situation, being unable to move at all is a trap.
        forward *= damage_.GetSpeedFactor();

        const float steer = std::clamp(input.steering, -1.0F, 1.0F);
        const float handBrake = input.handbrake ? 1.0F : 0.0F;
        physics.SetVehicleInput(vehicleHandle_, forward, steer, brake, handBrake);

        physics.Step(deltaSeconds);

        const float previousSpeed = speed_;
        position_ = physics.GetVehiclePosition(vehicleHandle_);
        yaw_ = physics.GetVehicleYaw(vehicleHandle_);

        const Vector3 velocity = physics.GetVehicleLinearVelocity(vehicleHandle_);
        speed_ = Vector3::Dot(velocity, GetForward());

        // plan_17 IG-17-015: a speed drop no brake could produce is a collision. Reading it back
        // from the speed the solver just produced needs nothing new from the physics layer and
        // cannot miss a contact Jolt resolved internally.
        lastImpactSeverity_ = damage_.RegisterFrame(previousSpeed, speed_, deltaSeconds);
        if (lastImpactSeverity_ > 0.0F)
        {
            Log::Info(LogCategory::Application,
                      "vehicle impact: integrity now " + std::to_string(damage_.GetIntegrity()) +
                          (damage_.IsDisabled() ? " (disabled)" : ""));
        }
    }
}
