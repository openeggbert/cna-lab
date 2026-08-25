#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Gameplay/VehicleConfig.hpp"
#include "IronGang/Gameplay/VehicleDamage.hpp"
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
        // Applies a tuning file's values (plan_17 IG-17-003). Must be called before the vehicle
        // body is created -- i.e. before the first Reset()/Restore() -- because the chassis, wheel
        // geometry, and mass are baked into the physics body at creation; a later call only
        // changes the speed limits. Never called at all means the built-in sedan, unchanged.
        void Configure(const VehicleConfig& config);
        [[nodiscard]] const VehicleConfig& GetConfig() const noexcept { return config_; }

        // Creates the vehicle body on first call; teleports it (with the given speed applied
        // along yaw's forward direction) on later calls, e.g. a mission reset.
        void Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics);
        void Update(float deltaSeconds, const VehicleInput& input, Physics::PhysicsWorld& physics);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] float GetSpeed() const noexcept { return speed_; }
        [[nodiscard]] float GetSpeedKph() const noexcept { return speed_ * 3.6F; }
        [[nodiscard]] Vector3 GetForward() const { return ForwardFromYaw(yaw_); }

        // plan_17 IG-17-015. Integrity runs 1 (undamaged) to 0 (wrecked); a wrecked sedan still
        // steers and rolls, slowly, rather than vanishing out from under the player. Reset()
        // repairs it -- starting or retrying a mission gives an intact car -- while Restore()
        // takes whatever the save recorded.
        [[nodiscard]] float GetIntegrity() const noexcept { return damage_.GetIntegrity(); }
        [[nodiscard]] bool IsDisabled() const noexcept { return damage_.IsDisabled(); }
        // Integrity lost in the most recent Update(), so the caller can react to the impact
        // itself rather than only to the running total.
        [[nodiscard]] float GetLastImpactSeverity() const noexcept { return lastImpactSeverity_; }
        void SetIntegrity(float integrity) noexcept { damage_.SetIntegrity(integrity); }

        // Save/load restore; also creates the vehicle body if this is called before Reset().
        void Restore(const Vector3& position, float yaw, float speed, Physics::PhysicsWorld& physics);

    private:
        void EnsureCreated(const Vector3& position, Physics::PhysicsWorld& physics);

        VehicleConfig config_;
        VehicleDamage damage_;
        float lastImpactSeverity_{0.0F};
        Physics::VehicleHandle vehicleHandle_;
        Vector3 position_{0.0F, 0.65F, 0.0F};
        float yaw_{0.0F};
        float speed_{0.0F};
        float previousForwardInput_{0.0F};
    };
}
