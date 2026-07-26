#pragma once

#include "IronShadows/Core/WorldTypes.hpp"

namespace IronShadows
{
    class PrototypeWorld;

    struct VehicleInput
    {
        float throttle{0.0F};
        float steering{0.0F};
        bool handbrake{false};
    };

    class VehicleController final
    {
    public:
        void Reset(const Vector3& spawnPosition, float yaw);
        void Update(float deltaSeconds, const VehicleInput& input, const PrototypeWorld& world);

        [[nodiscard]] const Vector3& GetPosition() const noexcept { return position_; }
        [[nodiscard]] float GetYaw() const noexcept { return yaw_; }
        [[nodiscard]] float GetSpeed() const noexcept { return speed_; }
        [[nodiscard]] float GetSpeedKph() const noexcept { return speed_ * 3.6F; }
        [[nodiscard]] Vector3 GetForward() const { return ForwardFromYaw(yaw_); }

        void Restore(const Vector3& position, float yaw, float speed);

    private:
        Vector3 position_{0.0F, 0.65F, 0.0F};
        float yaw_{0.0F};
        float speed_{0.0F};
        float engineAcceleration_{7.0F};
        float reverseAcceleration_{4.0F};
        float rollingDrag_{1.1F};
        float aerodynamicDrag_{0.035F};
        float maxForwardSpeed_{22.0F};
        float maxReverseSpeed_{6.0F};
        float steeringRate_{1.55F};
        float collisionRadius_{1.35F};
    };
}
