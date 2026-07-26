#pragma once

#include "IronShadows/Core/WorldTypes.hpp"

namespace IronShadows
{
    class PrototypeWorld;

    struct OnFootInput
    {
        float forward{0.0F};
        float strafe{0.0F};
        float turn{0.0F};
        bool sprint{false};
    };

    class PlayerController final
    {
    public:
        void Reset(const Vector3& spawnPosition, float yaw = 0.0F);
        void Update(float deltaSeconds, const OnFootInput& input, const PrototypeWorld& world);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] Vector3 GetForward() const { return ForwardFromYaw(yaw_); }

        void SetPosition(const Vector3& position) noexcept { position_ = position; }
        void SetYaw(float yaw) noexcept { yaw_ = yaw; }

    private:
        Vector3 position_{0.0F, 1.70F, 0.0F};
        float yaw_{0.0F};
        float walkSpeed_{4.2F};
        float sprintMultiplier_{1.65F};
        float turnSpeed_{2.0F};
        float collisionRadius_{0.35F};
    };
}
