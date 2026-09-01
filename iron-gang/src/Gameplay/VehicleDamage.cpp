#include "IronGang/Gameplay/VehicleDamage.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    float VehicleDamage::RegisterFrame(float previousSpeed, float currentSpeed, float deltaSeconds) noexcept
    {
        if (deltaSeconds <= 0.0F || !std::isfinite(previousSpeed) || !std::isfinite(currentSpeed))
        {
            return 0.0F;
        }

        // Compare magnitudes: reversing into a wall is as much a crash as driving into one, and a
        // sign change through zero is not itself an impact.
        const float speedLost = std::fabs(previousSpeed) - std::fabs(currentSpeed);
        if (speedLost <= 0.0F)
        {
            return 0.0F; // speeding up, or holding speed
        }

        const float deceleration = speedLost / deltaSeconds;
        if (deceleration <= settings_.impactDecelerationThreshold)
        {
            return 0.0F; // braking, coasting, or gravel under the wheels -- not a collision
        }

        // Severity is the part of the deceleration beyond what braking explains, expressed as the
        // speed that was lost too abruptly. A gentle nudge costs almost nothing; a full-speed
        // crash costs most of the car.
        const float severity = (deceleration - settings_.impactDecelerationThreshold) * deltaSeconds;
        const float lost = std::min(integrity_, severity * settings_.integrityLostPerImpactSpeed);
        integrity_ = std::clamp(integrity_ - lost, 0.0F, 1.0F);
        return lost;
    }

    float VehicleDamage::GetSpeedFactor() const noexcept
    {
        return settings_.minimumSpeedFactor +
               (1.0F - settings_.minimumSpeedFactor) * std::clamp(integrity_, 0.0F, 1.0F);
    }

    void VehicleDamage::SetIntegrity(float integrity) noexcept
    {
        integrity_ = std::isfinite(integrity) ? std::clamp(integrity, 0.0F, 1.0F) : 1.0F;
    }
}
