#pragma once

#include <cstddef>

namespace WolfCna
{
    enum class HudPortraitState
    {
        ReadyA,
        ReadyB,
        Wounded,
        Critical,
        Attacking,
        Hurt,
        Defeated,
        Count
    };

    [[nodiscard]] HudPortraitState SelectHudPortraitState(
        int health,
        bool recentlyHurt,
        bool attacking,
        bool defeated,
        unsigned idleFrame);

    [[nodiscard]] constexpr std::size_t HudPortraitIndex(HudPortraitState state)
    {
        return static_cast<std::size_t>(state);
    }
}
