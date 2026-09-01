#include "CopperBoots/InputActionAdapter.hpp"

#include <algorithm>
#include <cmath>

namespace CopperBoots
{
    PlayerInput InputActionAdapter::Sample(const InputSnapshot& snapshot) noexcept
    {
        pendingJump_ = pendingJump_ || (snapshot.Jump && !previousJump_);
        pendingAttack_ = pendingAttack_ ||
                         (snapshot.Attack && !previousAttack_);
        pendingPause_ = pendingPause_ || (snapshot.Pause && !previousPause_);
        previousJump_ = snapshot.Jump;
        previousAttack_ = snapshot.Attack;
        previousPause_ = snapshot.Pause;

        const bool opposingDigital = snapshot.Left && snapshot.Right;
        const int digitalMove = static_cast<int>(snapshot.Right) -
                                static_cast<int>(snapshot.Left);
        float move = static_cast<float>(digitalMove);
        if (!snapshot.Left && !snapshot.Right &&
            std::abs(snapshot.AnalogMove) > 0.20F)
            move = std::clamp(snapshot.AnalogMove, -1.0F, 1.0F);
        if (opposingDigital)
            move = 0.0F;

        PlayerInput result;
        result.Move = move;
        result.Run = snapshot.Run;
        result.JumpPressed = pendingJump_;
        result.JumpHeld = snapshot.Jump;
        result.AttackPressed = pendingAttack_;
        result.Aim = static_cast<int>(snapshot.AimDown) -
                     static_cast<int>(snapshot.AimUp);
        result.InteractHeld = snapshot.Interact;
        result.PausePressed = pendingPause_;
        return result;
    }

    void InputActionAdapter::ConsumeEdges() noexcept
    {
        pendingJump_ = false;
        pendingAttack_ = false;
        pendingPause_ = false;
    }
}
