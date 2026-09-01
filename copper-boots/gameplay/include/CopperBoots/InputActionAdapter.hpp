#pragma once

#include "CopperBoots/WorldSimulation.hpp"

namespace CopperBoots
{
    struct InputSnapshot
    {
        bool Left = false;
        bool Right = false;
        bool Run = false;
        bool Jump = false;
        bool Attack = false;
        bool AimUp = false;
        bool AimDown = false;
        bool Interact = false;
        bool Pause = false;
        float AnalogMove = 0.0F;
    };

    class InputActionAdapter
    {
    public:
        [[nodiscard]] PlayerInput Sample(const InputSnapshot& snapshot) noexcept;
        void ConsumeEdges() noexcept;

    private:
        bool previousJump_ = false;
        bool previousAttack_ = false;
        bool previousPause_ = false;
        bool pendingJump_ = false;
        bool pendingAttack_ = false;
        bool pendingPause_ = false;
    };
}
