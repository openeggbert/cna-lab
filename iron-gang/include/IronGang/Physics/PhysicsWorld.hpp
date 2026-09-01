#pragma once

#include "IronGang/Physics/PhysicsTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace IronGang::Physics
{
    // Exact workload snapshot at the Iron Gang/Jolt seam. Contact counts are current state;
    // steps and query counts cover work since the preceding snapshot and reset on capture.
    struct PhysicsProfileSnapshot
    {
        std::uint64_t bodyCount{0};
        std::uint64_t activeRigidBodyCount{0};
        std::uint64_t rigidBodyContactManifoldCount{0};
        std::uint64_t characterContactCount{0};
        std::uint64_t fixedStepCount{0};
        std::uint64_t publicRaycastCount{0};
        std::uint64_t characterCollisionUpdateCount{0};
        std::uint64_t vehicleWheelRaycastCount{0};
    };

    // Iron Gang-owned interface over the selected physics library (Jolt Physics, see
    // THIRD_PARTY.md and plan/plan_15-physics-integration.md IG-15-001..003). Jolt's own types
    // never appear in this header or in any calling code -- only PhysicsWorld.cpp includes Jolt
    // headers. Drives PlayerController's on-foot movement and VehicleController's driving
    // (plan_15/plan_16/plan_17).
    class PhysicsWorld final
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        // Fixed-step scheduling: Step() accumulates deltaSeconds and runs zero or more 1/60s
        // physics updates, decoupled from the render frame rate (IG-15-004).
        void Step(float deltaSeconds);

        // Profiling is opt-in so ordinary play does not maintain diagnostic contact/query state.
        // Capture returns current body/contact state and atomically consumes operation counters.
        void SetProfilingEnabled(bool enabled) noexcept;
        [[nodiscard]] PhysicsProfileSnapshot CaptureProfileSnapshot();

        [[nodiscard]] RigidBodyHandle CreateStaticBody(const ShapeDesc& shape, const Vector3& position);
        [[nodiscard]] RigidBodyHandle CreateDynamicBody(const ShapeDesc& shape, const Vector3& position, float mass);
        [[nodiscard]] RigidBodyHandle CreateTrigger(const ShapeDesc& shape, const Vector3& position);
        void DestroyBody(RigidBodyHandle handle);

        [[nodiscard]] Vector3 GetBodyPosition(RigidBodyHandle handle) const;
        void SetBodyLinearVelocity(RigidBodyHandle handle, const Vector3& velocity);
        [[nodiscard]] Vector3 GetBodyLinearVelocity(RigidBodyHandle handle) const;

        [[nodiscard]] RaycastHit Raycast(const Vector3& origin, const Vector3& direction, float maxDistance) const;

        // Number of rigid bodies currently registered with the physics system (static + dynamic +
        // triggers + vehicle chassis bodies). CharacterVirtual controllers are query-driven, not
        // registered rigid bodies, and their contacts are reported separately by the profiler.
        // Intended for tests/diagnostics, not gameplay logic.
        [[nodiscard]] std::size_t GetBodyCount() const;

        // Enter/exit events recorded since the last call; clears the internal queue.
        [[nodiscard]] std::vector<TriggerEvent> ConsumeTriggerEvents();

        // Character controller (JPH::CharacterVirtual): a capsule driven purely by a desired
        // horizontal+vertical velocity, matching PlayerController's existing input shape.
        [[nodiscard]] CharacterHandle CreateCharacter(const Vector3& position, float radius, float cylinderHalfHeight);
        void DestroyCharacter(CharacterHandle handle);
        void MoveCharacter(CharacterHandle handle, const Vector3& desiredVelocity, float deltaSeconds);
        [[nodiscard]] Vector3 GetCharacterPosition(CharacterHandle handle) const;
        [[nodiscard]] bool IsCharacterGrounded(CharacterHandle handle) const;
        // Teleport (mission reset, save/load restore, vehicle exit placement); zeroes velocity.
        void SetCharacterTransform(CharacterHandle handle, const Vector3& position, float yaw);

        // Vehicle (JPH::VehicleConstraint + WheeledVehicleController): a 4-wheel chassis. Wheel
        // positions are chassis-local, matching PrototypeRenderer's existing per-part offsets.
        [[nodiscard]] VehicleHandle CreateFourWheelVehicle(const Vector3& chassisHalfExtents,
                                                           float chassisMass,
                                                           const Vector3& position,
                                                           const std::array<Vector3, 4>& wheelLocalPositions,
                                                           float wheelRadius,
                                                           float wheelWidth);
        void DestroyVehicle(VehicleHandle handle);
        void SetVehicleInput(VehicleHandle handle, float forward, float steer, float brake, float handBrake);
        [[nodiscard]] Vector3 GetVehiclePosition(VehicleHandle handle) const;
        [[nodiscard]] float GetVehicleYaw(VehicleHandle handle) const;
        [[nodiscard]] Vector3 GetVehicleLinearVelocity(VehicleHandle handle) const;
        [[nodiscard]] std::array<VehicleWheelState, 4> GetVehicleWheelStates(VehicleHandle handle) const;
        // Teleport (mission reset, save/load restore); speed is applied along the yaw's forward
        // direction as the chassis's linear velocity.
        void SetVehicleTransform(VehicleHandle handle, const Vector3& position, float yaw, float speed);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
