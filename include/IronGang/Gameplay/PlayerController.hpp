#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Physics/PhysicsTypes.hpp"

namespace IronGang
{
    namespace Physics
    {
        class PhysicsWorld;
    }

    struct OnFootInput
    {
        float forward{0.0F};
        float strafe{0.0F};
        float turn{0.0F};
        bool sprint{false};
    };

    // Movement and world collision are driven by a Jolt CharacterVirtual capsule behind
    // Physics::PhysicsWorld (plan_15-physics-integration.md IG-15-021); PlayerController still
    // owns yaw and turn/sprint input handling itself and only asks physics to resolve the
    // resulting desired velocity against world geometry.
    class PlayerController final
    {
    public:
        // Creates the character body on first call; teleports it (zeroing velocity) on later
        // calls, e.g. a mission reset or a save/load restore.
        void Reset(const Vector3& spawnPosition, float yaw, Physics::PhysicsWorld& physics);
        void Update(float deltaSeconds, const OnFootInput& input, Physics::PhysicsWorld& physics);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] Vector3 GetForward() const { return ForwardFromYaw(yaw_); }
        [[nodiscard]] bool IsGrounded() const noexcept { return grounded_; }

        void SetPosition(const Vector3& position, Physics::PhysicsWorld& physics);
        void SetYaw(float yaw, Physics::PhysicsWorld& physics);

    private:
        Physics::CharacterHandle characterHandle_;
        Vector3 position_{0.0F, 1.70F, 0.0F};
        float yaw_{0.0F};
        bool grounded_{true};
        float walkSpeed_{4.2F};
        float sprintMultiplier_{1.65F};
        float turnSpeed_{2.0F};
        float collisionRadius_{0.35F};
        float capsuleCylinderHalfHeight_{0.5F};
    };
}
