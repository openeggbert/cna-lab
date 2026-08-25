#include "IronGang/Gameplay/Locomotion.hpp"

#include <algorithm>
#include <cmath>

namespace IronGang
{
    namespace
    {
        // Moves `value` toward `target` by at most `rate * deltaSeconds`, without overshooting --
        // an overshoot at low frame rates is exactly how a character jitters around a standstill.
        float MoveToward(float value, float target, float rate, float deltaSeconds) noexcept
        {
            const float step = rate * deltaSeconds;
            if (std::fabs(target - value) <= step)
            {
                return target;
            }
            return value + (target > value ? step : -step);
        }
    }

    void Locomotion::Update(float deltaSeconds,
                            float forwardInput,
                            float strafeInput,
                            float turnInput,
                            bool sprint) noexcept
    {
        if (!(deltaSeconds > 0.0F))
        {
            return;
        }

        float forward = std::clamp(forwardInput, -1.0F, 1.0F);
        float strafe = std::clamp(strafeInput, -1.0F, 1.0F);
        const float inputLength = std::sqrt(forward * forward + strafe * strafe);
        if (inputLength > 1.0F)
        {
            // Diagonal input must not be faster than straight input.
            forward /= inputLength;
            strafe /= inputLength;
        }

        const float targetSpeed = settings_.walkSpeed * (sprint ? settings_.sprintMultiplier : 1.0F);
        const float targetForward = forward * targetSpeed;
        const float targetStrafe = strafe * targetSpeed;

        // Slowing down uses the deceleration rate, speeding up the acceleration rate -- decided
        // per axis, so releasing forward while still strafing does not brake the strafe.
        const float forwardRate = std::fabs(targetForward) < std::fabs(forwardVelocity_)
                                      ? settings_.deceleration
                                      : settings_.acceleration;
        const float strafeRate = std::fabs(targetStrafe) < std::fabs(strafeVelocity_)
                                     ? settings_.deceleration
                                     : settings_.acceleration;
        forwardVelocity_ = MoveToward(forwardVelocity_, targetForward, forwardRate, deltaSeconds);
        strafeVelocity_ = MoveToward(strafeVelocity_, targetStrafe, strafeRate, deltaSeconds);

        const float targetTurnRate = std::clamp(turnInput, -1.0F, 1.0F) * settings_.turnSpeed;
        turnRate_ = MoveToward(turnRate_, targetTurnRate, settings_.turnAcceleration, deltaSeconds);
    }

    float Locomotion::GetSpeed() const noexcept
    {
        return std::sqrt(forwardVelocity_ * forwardVelocity_ + strafeVelocity_ * strafeVelocity_);
    }

    bool Locomotion::IsMoving() const noexcept
    {
        // Well below walking pace: an animation or a footstep sound keyed off this must not
        // flicker while the character eases to a stop.
        return GetSpeed() > 0.05F;
    }

    void Locomotion::Stop() noexcept
    {
        forwardVelocity_ = 0.0F;
        strafeVelocity_ = 0.0F;
        turnRate_ = 0.0F;
    }
}
