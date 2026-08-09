#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Physics/PhysicsTypes.hpp"

namespace IronGang
{
    namespace Physics
    {
        class PhysicsWorld;
    }

    struct VehicleInput
    {
        float throttle{0.0F};
        float steering{0.0F};
        bool handbrake{false};
    };

    // Driving is a Jolt VehicleConstraint (4-wheel raycast vehicle) behind Physics::PhysicsWorld
    // (plan_15-physics-integration.md IG-15-025); position/yaw/speed below are read back from
    // physics every frame, not simulated by VehicleController itself. Chassis/wheel dimensions
    // match PrototypeRenderer's existing body/cabin/windshield/wheel visual offsets so the
    // physics chassis and the rendered sedan line up without a separate visual-only transform.
    class VehicleController final
    {
    public:
        // Creates the vehicle body on first call; teleports it (with the given speed applied
        // along yaw's forward direction) on later calls, e.g. a mission reset.
        void Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics);
        void Update(float deltaSeconds, const VehicleInput& input, Physics::PhysicsWorld& physics);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] float GetSpeed() const noexcept { return speed_; }
        [[nodiscard]] float GetSpeedKph() const noexcept { return speed_ * 3.6F; }
        [[nodiscard]] Vector3 GetForward() const { return ForwardFromYaw(yaw_); }

        // Save/load restore; also creates the vehicle body if this is called before Reset().
        void Restore(const Vector3& position, float yaw, float speed, Physics::PhysicsWorld& physics);

    private:
        void EnsureCreated(const Vector3& position, Physics::PhysicsWorld& physics);

        Physics::VehicleHandle vehicleHandle_;
        Vector3 position_{0.0F, 0.65F, 0.0F};
        float yaw_{0.0F};
        float speed_{0.0F};
        float previousForwardInput_{0.0F};
        float maxForwardSpeed_{22.0F};
        float maxReverseSpeed_{6.0F};

        // CNA's Vector3 has no constexpr constructor, so the chassis/wheel geometry constants
        // live as local consts in VehicleController.cpp's EnsureCreated() instead of here.
        static constexpr float kChassisMass{1400.0F};
        static constexpr float kWheelRadius{0.33F};
        static constexpr float kWheelWidth{0.3F};
    };
}
