#include "HudStatus.hpp"

namespace WolfCna
{
    HudPortraitState SelectHudPortraitState(
        int health,
        bool recentlyHurt,
        bool attacking,
        bool defeated,
        unsigned idleFrame)
    {
        if (defeated || health <= 0)
            return HudPortraitState::Defeated;
        if (recentlyHurt)
            return HudPortraitState::Hurt;
        if (attacking)
            return HudPortraitState::Attacking;
        if (health <= 20)
            return HudPortraitState::Critical;
        if (health <= 50)
            return HudPortraitState::Wounded;
        return (idleFrame & 1u) == 0u
            ? HudPortraitState::ReadyA
            : HudPortraitState::ReadyB;
    }
}
